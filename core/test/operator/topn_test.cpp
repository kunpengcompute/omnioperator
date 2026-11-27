/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */

#include <vector>
#include <iostream>
#include <chrono>
#include "gtest/gtest.h"
#include "operator/topn/topn.h"
#include "operator/omni_id_type_vector_traits.h"
#include "vector/vector_helper.h"
#include "util/perf_util.h"
#include "util/test_util.h"
#include "type/data_type.h"

using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace std;
using namespace omniruntime::TestUtil;

namespace TopnTest {
using IntVector = NativeAndVectorType<DataTypeId::OMNI_INT>::vector;
using LongVector = NativeAndVectorType<DataTypeId::OMNI_LONG>::vector;
using DoubleVector = NativeAndVectorType<DataTypeId::OMNI_DOUBLE>::vector;
using ShortVector = NativeAndVectorType<DataTypeId::OMNI_SHORT>::vector;
using CharVector = NativeAndVectorType<DataTypeId::OMNI_CHAR>::vector;
using VarcharVector = NativeAndVectorType<DataTypeId::OMNI_VARCHAR>::vector;

void DeleteTopNOperatorFactory(TopNOperatorFactory *topNOperatorFactory)
{
    if (topNOperatorFactory != nullptr) {
        delete topNOperatorFactory;
    }
}


TEST(NativeOmniTopNOperatorTest, TestTopNAscOneColumnPerformance)
{
    // construct input data
    const int32_t dataSize = 100000;
    const int32_t expectedDataSize = 5;

    // prepare data
    int32_t *data0 = new int32_t[dataSize];
    for (int i = 0; i < dataSize; ++i) {
        data0[i] = dataSize - i;
    }

    VectorBatch *inputVecBatch1 = new VectorBatch(dataSize);

    auto column0 = new Vector<int32_t>(dataSize);
    for (int32_t i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
    }
    std::vector<DataTypePtr> types = { IntType() };
    inputVecBatch1->Append(column0);
    VectorBatch *inputVecBatch2 = DuplicateVectorBatch(inputVecBatch1);


    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    auto *perfUtil = new PerfUtil();
    perfUtil->Init();
    perfUtil->Reset();
    perfUtil->Start();
    auto s = clock();

    topNOperator->AddInput(inputVecBatch1);
    perfUtil->Stop();
    long instCount = perfUtil->GetData();
    if (instCount != -1) {
        printf("TopN with OmniJit, used %lld instructions\n", perfUtil->GetData());
    }

    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    auto e = clock();
    cout << "topn with OmniJit performance takes: " << (double)(e - s) / CLOCKS_PER_SEC << endl;

    int32_t expectData1[expectedDataSize] = {1, 2, 3, 4, 5};
    auto expectCol1 = new Vector<int32_t>(expectedDataSize);
    for (int32_t i = 0; i < expectedDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    TopNOperatorFactory *topNOperatorFactory2 =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator2 = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory2));

    perfUtil->Init();
    perfUtil->Reset();
    perfUtil->Start();
    auto s2 = clock();

    topNOperator2->AddInput(inputVecBatch2);
    perfUtil->Stop();
    instCount = perfUtil->GetData();
    if (instCount != -1) {
        printf("TopN without OmniJit, used %lld instructions\n", perfUtil->GetData());
    }

    VectorBatch *outputVectorBatch2;
    topNOperator2->GetOutput(&outputVectorBatch2);

    auto e2 = clock();
    cout << "topn without OmniJit performance takes: " << (double)(e2 - s2) / CLOCKS_PER_SEC << endl;

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch2, expectVectorBatch));

    delete perfUtil;
    delete[] data0;
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    omniruntime::op::Operator::DeleteOperator(topNOperator2);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    DeleteTopNOperatorFactory(topNOperatorFactory2);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch2);
}

TEST(NativeOmniTopNOperatorTest, TestTopNInstruct)
{
    // construct input data
    const int32_t dataSize = 10000000;
    const int32_t expectedDataSize = 5;

    // prepare data
    int32_t *data0 = new int32_t[dataSize];
    for (int i = 0; i < dataSize; ++i) {
        data0[i] = dataSize % expectedDataSize;
    }

    VectorBatch *inputVecBatch1 = new VectorBatch(dataSize);
    auto column0 = new Vector<int32_t>(dataSize);
    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
    }

    inputVecBatch1->Append(column0);
    VectorBatch *inputVecBatch2 = new VectorBatch(dataSize);
    auto column2 = new Vector<int32_t>(dataSize);
    for (int i = 0; i < dataSize; i++) {
        column2->SetValue(i, data0[i]);
    }
    inputVecBatch2->Append(column2);

    std::vector<DataTypePtr> types = { IntType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);
    auto s = clock();

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    auto *perfUtil = new PerfUtil();
    perfUtil->Init();
    perfUtil->Reset();
    perfUtil->Start();
    topNOperator->AddInput(inputVecBatch1);
    perfUtil->Stop();
    long instCount = perfUtil->GetData();
    if (instCount != -1) {
        printf("TopN with OmniJit, used %lld instructions\n", perfUtil->GetData());
    }
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    auto e = clock();
    cout << "topn with OmniJit performance takes: " << (double)(e - s) / CLOCKS_PER_SEC << endl;

    TopNOperatorFactory *topNOperatorFactoryWithoutJit =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);
    auto s2 = clock();
    auto topNOp = topNOperatorFactoryWithoutJit->CreateOperator(nullptr);
    perfUtil->Init();
    perfUtil->Reset();
    perfUtil->Start();
    topNOp->AddInput(inputVecBatch2);
    perfUtil->Stop();
    instCount = perfUtil->GetData();
    if (instCount != -1) {
        printf("TopN without OmniJit, used %lld instructions\n", perfUtil->GetData());
    }
    VectorBatch *outputVectorBatchWithoutJit;
    topNOp->GetOutput(&outputVectorBatchWithoutJit);
    auto e2 = clock();
    cout << "topn without OmniJit performance takes: " << (double)(e2 - s2) / CLOCKS_PER_SEC << endl;

    int32_t expectData1[expectedDataSize] = {7, 37, 51, 95, 95};
    auto expectCol1 = new Vector<int32_t>(expectedDataSize);
    for (int i = 0; i < expectedDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);

    delete[] data0;
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    DeleteTopNOperatorFactory(topNOperatorFactoryWithoutJit);
    omniruntime::op::Operator::DeleteOperator(topNOp);
    delete perfUtil;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatchWithoutJit);
}
TEST(NativeOmniTopNOperatorTest, TestTopNAscOneColumnPerformanceVarChar)
{
    // construct input data
    const int32_t dataSize = 1000;
    const int32_t expectedDataSize = 5;

    // prepare data
    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    auto column0 = new Vector<LargeStringContainer<std::string_view>>(dataSize);
    for (int i = 0; i < dataSize; ++i) {
        std::string str = std::to_string(i % 10);
        std::string_view strView(str.data(), str.size());
        column0->SetValue(i, strView);
    }
    inputVecBatch->Append(column0);

    std::vector<DataTypePtr> types = { VarcharType(3) };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    auto s = clock();

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    auto e = clock();
    cout << "topn performance takes: " << (double)(e - s) / CLOCKS_PER_SEC << endl;

    string expectData1[expectedDataSize] = {"0", "0", "0", "0", "0"};
    auto expectCol1 = new Vector<LargeStringContainer<std::string_view>>(expectedDataSize);
    for (int i = 0; i < 5; ++i) {
        string_view str(expectData1[i].data(), expectData1[i].size());
        expectCol1->SetValue(i, str);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscOneColumn)
{
    // construct input data
    const int32_t dataSize = 7;
    const int32_t expectedDataSize = 5;

    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 4, 5, 2, 3};

    omniruntime::vec::VectorBatch *inputVecBatch = new VectorBatch(7);
    auto column0 = new Vector<int32_t>(dataSize);
    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
    }

    inputVecBatch->Append(column0);
    std::vector<DataTypePtr> types = { IntType() };
    omniruntime::type::DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[expectedDataSize] = {0, 1, 2, 2, 3};
    auto expectCol1 = new Vector<int32_t>(expectedDataSize);

    for (int i = 0; i < expectedDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(5);
    expectVectorBatch->Append(expectCol1);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNOneColumnWithOffset)
{
    // construct input data
    const int32_t dataSize = 7;
    const int32_t expectedDataSize = 5;
    const int32_t offsetSize = 2;

    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 4, 5, 2, 3};

    omniruntime::vec::VectorBatch *inputVecBatch = new VectorBatch(7);
    auto column0 = new Vector<int32_t>(dataSize);
    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
    }

    inputVecBatch->Append(column0);
    std::vector<DataTypePtr> types = { IntType() };
    omniruntime::type::DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
            new TopNOperatorFactory(sourceTypes, expectedDataSize, offsetSize, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[expectedDataSize] = {2, 2, 3};
    auto expectCol1 = new Vector<int32_t>(expectedDataSize - offsetSize);

    for (int i = 0; i < expectedDataSize - offsetSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(3);
    expectVectorBatch->Append(expectCol1);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscOneColumnVarChar)
{
    // construct input data
    const int32_t dataSize = 7;
    const int32_t expectedDataSize = 5;

    // prepare data
    string data0[dataSize] = {"0", "1", "2", "4", "5", "2", "3"};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    auto column0Base = VectorHelper::CreateStringVector(dataSize);
    auto *column0 = (VarcharVector *)column0Base;
    for (int i = 0; i < dataSize; ++i) {
        string_view str(data0[i]);
        column0->SetValue(i, str);
    }
    inputVecBatch->Append(column0Base);

    std::vector<DataTypePtr> types = { VarcharType(3) };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    string expectData1[expectedDataSize] = {"0", "1", "2", "2", "3"};
    auto expectCol1Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto *expectCol1 = (VarcharVector *)expectCol1Base;

    for (int i = 0; i < 5; ++i) {
        string_view str(expectData1[i]);
        expectCol1->SetValue(i, str);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1Base);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));


    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscOneColumnChar)
{
    // construct input data
    const int32_t dataSize = 7;
    const int32_t expectedDataSize = 5;

    // prepare data
    string data0[dataSize] = {"0", "1", "2", "4", "5", "2", "3"};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    auto column0Base = VectorHelper::CreateStringVector(dataSize);
    auto *column0 = (VarcharVector *)column0Base;
    for (int i = 0; i < dataSize; ++i) {
        string_view str(data0[i]);
        column0->SetValue(i, str);
    }
    inputVecBatch->Append(column0Base);

    std::vector<DataTypePtr> types = { CharType(3) };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    string expectData1[expectedDataSize] = {"0", "1", "2", "2", "3"};
    auto expectCol1Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto *expectCol1 = (VarcharVector *)expectCol1Base;

    for (int i = 0; i < 5; ++i) {
        string_view str(expectData1[i]);
        expectCol1->SetValue(i, str);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1Base);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));


    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescOneColumn)
{
    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;

    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
    }

    inputVecBatch->Append(column0);
    std::vector<DataTypePtr> types = { IntType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[expectedDataSize] = {2, 2, 1, 1, 0};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    for (int i = 0; i < expectedDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescOneColumnVarChar)
{
    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;

    // prepare data
    std::string data0[dataSize] = {"0", "1", "2", "0", "1", "2"};
    VectorBatch *inputVecBatch = new VectorBatch(dataSize);

    auto column0Base = VectorHelper::CreateStringVector(dataSize);
    auto *column0 = (VarcharVector *)column0Base;
    for (int i = 0; i < dataSize; ++i) {
        string_view str(data0[i]);
        column0->SetValue(i, str);
    }
    inputVecBatch->Append(column0Base);

    std::vector<DataTypePtr> types = { VarcharType(3) };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    std::string expectData1[expectedDataSize] = {"2", "2", "1", "1", "0"};

    auto expectCol1Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto *expectCol1 = (VarcharVector *)expectCol1Base;
    for (int i = 0; i < 5; ++i) {
        string_view str(expectData1[i]);
        expectCol1->SetValue(i, str);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1Base);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescOneColumnChar)
{
    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;

    // prepare data
    std::string data0[dataSize] = {"0", "1", "2", "0", "1", "2"};
    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    auto column0Base = VectorHelper::CreateStringVector(dataSize);
    auto *column0 = (VarcharVector *)column0Base;
    for (int i = 0; i < dataSize; ++i) {
        string_view str(data0[i]);
        column0->SetValue(i, str);
    }
    inputVecBatch->Append(column0Base);

    std::vector<DataTypePtr> types = { CharType(3) };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    std::string expectData1[expectedDataSize] = {"2", "2", "1", "1", "0"};
    auto expectCol1Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto *expectCol1 = (VarcharVector *)expectCol1Base;
    for (int i = 0; i < 5; ++i) {
        string_view str(expectData1[i]);
        expectCol1->SetValue(i, str);
    }
    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1Base);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumn)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1, 0};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    DoubleVector *column2 = new DoubleVector(dataSize);
    ShortVector *column3 = new ShortVector(dataSize);

    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column2->SetValue(i, data2[i]);
        column3->SetValue(i, data3[i]);
    }
    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);
    inputVecBatch->Append(column2);
    inputVecBatch->Append(column3);

    std::vector<DataTypePtr> types = { IntType(), LongType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[expectedDataSize] = {0, 0, 1, 1, 2};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    int64_t expectData2[expectedDataSize] = {0, 3, 1, 4, 2};
    LongVector *expectCol2 = new LongVector(expectedDataSize);
    double expectData3[expectedDataSize] = {6.6, 3.3, 5.5, 2.2, 4.4};
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);
    int16_t expectData4[expectedDataSize] = {5, 2, 4, 1, 3};
    ShortVector *expectCol4 = new ShortVector(expectedDataSize);
    for (int i = 0; i < expectedDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
        expectCol4->SetValue(i, expectData4[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);
    expectVectorBatch->Append(expectCol4);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNMultiColumnWithOffset)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;

    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1, 0};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    DoubleVector *column2 = new DoubleVector(dataSize);
    ShortVector *column3 = new ShortVector(dataSize);

    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column2->SetValue(i, data2[i]);
        column3->SetValue(i, data3[i]);
    }
    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);
    inputVecBatch->Append(column2);
    inputVecBatch->Append(column3);

    std::vector<DataTypePtr> types = { IntType(), LongType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;
    const int32_t offsetSize = 2;
    const int32_t realDataSize = expectedDataSize - offsetSize;

    TopNOperatorFactory *topNOperatorFactory =
            new TopNOperatorFactory(sourceTypes, expectedDataSize, offsetSize, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[realDataSize] = {1, 1, 2};
    IntVector *expectCol1 = new IntVector(realDataSize);
    int64_t expectData2[realDataSize] = { 1, 4, 2};
    LongVector *expectCol2 = new LongVector(realDataSize);
    double expectData3[realDataSize] = {5.5, 2.2, 4.4};
    DoubleVector *expectCol3 = new DoubleVector(realDataSize);
    int16_t expectData4[realDataSize] = {4, 1, 3};
    ShortVector *expectCol4 = new ShortVector(realDataSize);
    for (int i = 0; i < realDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
        expectCol4->SetValue(i, expectData4[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(realDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);
    expectVectorBatch->Append(expectCol4);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnVarChar)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    std::string data1[dataSize] = {"0", "1", "2", "3", "4", "5"};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    auto column1Base = VectorHelper::CreateStringVector(dataSize);
    auto *column1 = (VarcharVector *)column1Base;
    DoubleVector *column2 = new DoubleVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        std::string_view str(data1[i]);
        column1->SetValue(i, str);
        column2->SetValue(i, data2[i]);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1Base);
    inputVecBatch->Append(column2);

    std::vector<DataTypePtr> types = { IntType(), VarcharType(3), DoubleType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    IntVector *expectCol1 = new IntVector(expectedDataSize);
    auto expectCol2Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto expectCol2 = (VarcharVector *)expectCol2Base;
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    int32_t expectData1[expectedDataSize] = {0, 0, 1, 1, 2};
    std::string expectData2[expectedDataSize] = {"0", "3", "1", "4", "2"};
    double expectData3[expectedDataSize] = {6.6, 3.3, 5.5, 2.2, 4.4};

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        std::string_view str(expectData2[i]);
        expectCol2->SetValue(i, str);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2Base);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnChar)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    std::string data1[dataSize] = {"0", "1", "2", "3", "4", "5"};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};


    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    auto column1Base = VectorHelper::CreateStringVector(dataSize);
    auto *column1 = (VarcharVector *)column1Base;
    DoubleVector *column2 = new DoubleVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        std::string_view str(data1[i]);
        column1->SetValue(i, str);
        column2->SetValue(i, data2[i]);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1Base);
    inputVecBatch->Append(column2);

    std::vector<DataTypePtr> types = { IntType(), CharType(3), DoubleType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    IntVector *expectCol1 = new IntVector(expectedDataSize);
    auto expectCol2Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto expectCol2 = (VarcharVector *)expectCol2Base;
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    int32_t expectData1[expectedDataSize] = {0, 0, 1, 1, 2};
    std::string expectData2[expectedDataSize] = {"0", "3", "1", "4", "2"};
    double expectData3[expectedDataSize] = {6.6, 3.3, 5.5, 2.2, 4.4};

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        std::string_view str(expectData2[i]);
        expectCol2->SetValue(i, str);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2Base);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescMultiColumn)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1, 0};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    DoubleVector *column2 = new DoubleVector(dataSize);
    ShortVector *column3 = new ShortVector(dataSize);

    for (int i = 0; i < dataSize; i++) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column2->SetValue(i, data2[i]);
        column3->SetValue(i, data3[i]);
    }
    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);
    inputVecBatch->Append(column2);
    inputVecBatch->Append(column3);

    std::vector<DataTypePtr> types = { IntType(), LongType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, false};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    int32_t expectData1[expectedDataSize] = {0, 0, 1, 1, 2};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    int64_t expectData2[expectedDataSize] = {3, 0, 4, 1, 5};
    LongVector *expectCol2 = new LongVector(expectedDataSize);
    double expectData3[expectedDataSize] = {3.3, 6.6, 2.2, 5.5, 1.1};
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);
    int16_t expectData4[expectedDataSize] = {2, 5, 1, 4, 0};
    ShortVector *expectCol4 = new ShortVector(expectedDataSize);
    for (int i = 0; i < expectedDataSize; i++) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
        expectCol4->SetValue(i, expectData4[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);
    expectVectorBatch->Append(expectCol4);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescMultiColumnVarChar)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    std::string data1[dataSize] = {"0", "1", "2", "3", "4", "5"};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    auto column1Base = VectorHelper::CreateStringVector(dataSize);
    auto *column1 = (VarcharVector *)column1Base;
    DoubleVector *column2 = new DoubleVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        std::string_view str(data1[i]);
        column1->SetValue(i, str);
        column2->SetValue(i, data2[i]);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1Base);
    inputVecBatch->Append(column2);

    std::vector<DataTypePtr> types = { IntType(), VarcharType(3), DoubleType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, false};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);
    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    IntVector *expectCol1 = new IntVector(expectedDataSize);
    auto expectCol2Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto expectCol2 = (VarcharVector *)expectCol2Base;
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    int32_t expectData1[expectedDataSize] = {0, 0, 1, 1, 2};
    std::string expectData2[expectedDataSize] = {"3", "0", "4", "1", "5"};
    double expectData3[expectedDataSize] = {3.3, 6.6, 2.2, 5.5, 1.1};
    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        std::string_view str(expectData2[i]);
        expectCol2->SetValue(i, str);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2Base);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescMultiColumnChar)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    std::string data1[dataSize] = {"0", "1", "2", "3", "4", "5"};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    auto column1Base = VectorHelper::CreateStringVector(dataSize);
    auto *column1 = (VarcharVector *)column1Base;
    DoubleVector *column2 = new DoubleVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        std::string_view str(data1[i]);
        column1->SetValue(i, str);
        column2->SetValue(i, data2[i]);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1Base);
    inputVecBatch->Append(column2);

    std::vector<DataTypePtr> types = { IntType(), CharType(3), DoubleType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, false};
    int32_t nullFirsts[2] = {false, false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    IntVector *expectCol1 = new IntVector(expectedDataSize);
    auto expectCol2Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto expectCol2 = (VarcharVector *)expectCol2Base;
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    int32_t expectData1[expectedDataSize] = {0, 0, 1, 1, 2};
    std::string expectData2[expectedDataSize] = {"3", "0", "4", "1", "5"};
    double expectData3[expectedDataSize] = {3.3, 6.6, 2.2, 5.5, 1.1};

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        std::string_view str(expectData2[i]);
        expectCol2->SetValue(i, str);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2Base);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnNullFirstAndDictionaryVecVarChar)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    std::string data1[dataSize] = {"0", "1", "2", "3", "4", "5"};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    auto column1Base = VectorHelper::CreateStringVector(dataSize);
    auto *column1 = (VarcharVector *)column1Base;

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        std::string_view str(data1[i]);
        column1->SetValue(i, str);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1Base);

    std::vector<DataTypePtr> types = { IntType(), VarcharType(3), DoubleType() };
    DataTypes sourceTypes(types);

    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {true, true};
    const int32_t expectedDataSize = 5;
    inputVecBatch->Get(0)->SetNull(dataSize - 1);
    inputVecBatch->Get(1)->SetNull(dataSize - 1);

    int32_t ids[] = {0, 1, 2, 3, 4, 5};
    DataTypePtr dataType = sourceTypes.Get()[2];
    auto column2 = CreateDictionaryVector(*dataType, dataSize, ids, dataSize, data2);
    inputVecBatch->Append(column2);

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    IntVector *expectCol1 = new IntVector(expectedDataSize);
    auto expectCol2Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto expectCol2 = (VarcharVector *)expectCol2Base;
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    int32_t expectData1[expectedDataSize] = {2, 0, 0, 1, 1};
    std::vector<std::string> expectData2 = { "", "0", "3", "1", "4" };
    double expectData3[expectedDataSize] = {1.1, 6.6, 3.3, 5.5, 2.2};

    expectCol1->SetNull(0);
    expectCol2->SetNull(0);
    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        std::string_view str(expectData2[i]);
        expectCol2->SetValue(i, str);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2Base);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));
    EXPECT_TRUE(outputVectorBatch->Get(0)->IsNull(0));
    EXPECT_TRUE(!outputVectorBatch->Get(0)->IsNull(2));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnNullFirstAndDictionaryChar)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    std::string data1[dataSize] = {"0", "1", "2", "3", "4", "5"};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    auto column1Base = VectorHelper::CreateStringVector(dataSize);
    auto *column1 = (VarcharVector *)column1Base;

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        std::string_view str(data1[i]);
        column1->SetValue(i, str);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1Base);

    std::vector<DataTypePtr> types = { IntType(), VarcharType(3), DoubleType() };
    DataTypes sourceTypes(types);

    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {true, true};
    const int32_t expectedDataSize = 5;
    inputVecBatch->Get(0)->SetNull(dataSize - 1);
    inputVecBatch->Get(1)->SetNull(dataSize - 1);

    int32_t ids[] = {0, 1, 2, 3, 4, 5};
    DataTypePtr dataType = sourceTypes.Get()[2];
    auto column2 = CreateDictionaryVector(*dataType, dataSize, ids, dataSize, data2);
    inputVecBatch->Append(column2);

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    IntVector *expectCol1 = new IntVector(expectedDataSize);
    auto expectCol2Base = VectorHelper::CreateStringVector(expectedDataSize);
    auto expectCol2 = (VarcharVector *)expectCol2Base;
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    int32_t expectData1[expectedDataSize] = {2, 0, 0, 1, 1};
    std::vector<std::string> expectData2 = { "5", "0", "3", "1", "4" };
    double expectData3[expectedDataSize] = {1.1, 6.6, 3.3, 5.5, 2.2};

    expectCol1->SetNull(0);
    expectCol2->SetNull(0);
    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        std::string_view str(expectData2[i]);
        expectCol2->SetValue(i, str);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2Base);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));
    EXPECT_TRUE(outputVectorBatch->Get(0)->IsNull(0));
    EXPECT_TRUE(!outputVectorBatch->Get(0)->IsNull(2));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNDescMultiColumnSortOnlyOneColumn)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    DoubleVector *column2 = new DoubleVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column2->SetValue(i, data2[i]);
    }

    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);
    inputVecBatch->Append(column2);

    std::vector<DataTypePtr> types = { IntType(), LongDataType::Instance(), DoubleType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    int32_t expectData1[expectedDataSize] = {2, 1, 0, 2, 1};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    int64_t expectData2[expectedDataSize] = {5, 4, 3, 2, 1};
    LongVector *expectCol2 = new LongVector(expectedDataSize);
    double expectData3[expectedDataSize] = {1.1, 2.2, 3.3, 4.4, 5.5};
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
    }

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);;
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnNullFirstAndDictionaryVec)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1, 0};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    ShortVector *column3 = new ShortVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column3->SetValue(i, data3[i]);
    }

    column0->SetNull(dataSize - 1);
    column1->SetNull(dataSize - 1);
    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);

    std::vector<DataTypePtr> types = { IntType(), LongType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {true, true};

    const int32_t expectedDataSize = 5;
    int32_t ids[] = {0, 1, 2, 3, 4, 5};
    DataTypePtr dataType = sourceTypes.Get()[2];
    auto column2 = CreateDictionaryVector(*dataType, dataSize, ids, dataSize, data2);
    inputVecBatch->Append(column2);
    inputVecBatch->Append(column3);

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[expectedDataSize] = {2, 0, 0, 1, 1};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    int64_t expectData2[expectedDataSize] = {5, 0, 3, 1, 4};
    LongVector *expectCol2 = new LongVector(expectedDataSize);
    double expectData3[expectedDataSize] = {1.1, 6.6, 3.3, 5.5, 2.2};
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);
    int16_t expectData4[expectedDataSize] = {0, 5, 2, 4, 1};
    ShortVector *expectCol4 = new ShortVector(expectedDataSize);

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
        expectCol4->SetValue(i, expectData4[i]);
    }
    expectCol1->SetNull(0);
    expectCol2->SetNull(0);

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);
    expectVectorBatch->Append(expectCol4);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));
    EXPECT_TRUE(outputVectorBatch->Get(0)->IsNull(0));
    EXPECT_TRUE(!outputVectorBatch->Get(0)->IsNull(2));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnNullFirst)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1, 0};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    DoubleVector *column2 = new DoubleVector(dataSize);
    ShortVector *column3 = new ShortVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column2->SetValue(i, data2[i]);
        column3->SetValue(i, data3[i]);
    }
    column0->SetNull(dataSize - 1);
    column1->SetNull(dataSize - 1);
    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);
    inputVecBatch->Append(column2);
    inputVecBatch->Append(column3);

    std::vector<DataTypePtr> types = { IntType(), LongType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {true, true};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);

    int32_t expectData1[expectedDataSize] = {2, 0, 0, 1, 1};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    int64_t expectData2[expectedDataSize] = {5, 0, 3, 1, 4};
    LongVector *expectCol2 = new LongVector(expectedDataSize);
    double expectData3[expectedDataSize] = {1.1, 6.6, 3.3, 5.5, 2.2};
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);
    int16_t expectData4[expectedDataSize] = {0, 5, 2, 4, 1};
    ShortVector *expectCol4 = new ShortVector(expectedDataSize);

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
        expectCol4->SetValue(i, expectData4[i]);
    }
    expectCol1->SetNull(0);
    expectCol2->SetNull(0);
    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);
    expectVectorBatch->Append(expectCol4);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));
    EXPECT_TRUE(outputVectorBatch->Get(0)->IsNull(0));
    EXPECT_TRUE(!outputVectorBatch->Get(0)->IsNull(2));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}


TEST(NativeOmniTopNOperatorTest, TestTopNAscMultiColumnNullLast)
{
    using namespace omniruntime::op;
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {-1, 1, -1, 0, 1, -1};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, -1};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1, 0};

    VectorBatch *inputVecBatch = new VectorBatch(dataSize);
    IntVector *column0 = new IntVector(dataSize);
    LongVector *column1 = new LongVector(dataSize);
    DoubleVector *column2 = new DoubleVector(dataSize);
    ShortVector *column3 = new ShortVector(dataSize);

    for (int i = 0; i < dataSize; ++i) {
        column0->SetValue(i, data0[i]);
        column1->SetValue(i, data1[i]);
        column2->SetValue(i, data2[i]);
        column3->SetValue(i, data3[i]);
    }
    column0->SetNull(5);
    column0->SetNull(2);
    column0->SetNull(0);
    column1->SetNull(5);
    inputVecBatch->Append(column0);
    inputVecBatch->Append(column1);
    inputVecBatch->Append(column2);
    inputVecBatch->Append(column3);

    std::vector<DataTypePtr> types = { IntType(), LongType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t sortCols[2] = {0, 1};
    int32_t ascendings[2] = {true, true};
    int32_t nullFirsts[2] = {false, true};
    const int32_t expectedDataSize = 5;

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(inputVecBatch);
    VectorBatch *outputVectorBatch;
    topNOperator->GetOutput(&outputVectorBatch);
    int32_t expectData1[expectedDataSize] = {0, 1, 1, -1, -1};
    IntVector *expectCol1 = new IntVector(expectedDataSize);
    int64_t expectData2[expectedDataSize] = {3, 1, 4, -1, 0};
    LongVector *expectCol2 = new LongVector(expectedDataSize);
    double expectData3[expectedDataSize] = {3.3, 5.5, 2.2, 1.1, 6.6};
    DoubleVector *expectCol3 = new DoubleVector(expectedDataSize);
    int16_t expectData4[expectedDataSize] = {2, 4, 1, 0, 5};
    ShortVector *expectCol4 = new ShortVector(expectedDataSize);

    for (int i = 0; i < expectedDataSize; ++i) {
        expectCol1->SetValue(i, expectData1[i]);
        expectCol2->SetValue(i, expectData2[i]);
        expectCol3->SetValue(i, expectData3[i]);
        expectCol4->SetValue(i, expectData4[i]);
    }
    expectCol1->SetNull(3);
    expectCol1->SetNull(4);
    expectCol2->SetNull(3);

    VectorBatch *expectVectorBatch = new VectorBatch(expectedDataSize);
    expectVectorBatch->Append(expectCol1);
    expectVectorBatch->Append(expectCol2);
    expectVectorBatch->Append(expectCol3);
    expectVectorBatch->Append(expectCol4);

    EXPECT_TRUE(VecBatchMatch(outputVectorBatch, expectVectorBatch));
    EXPECT_TRUE(outputVectorBatch->Get(0)->IsNull(3));
    EXPECT_TRUE(!outputVectorBatch->Get(0)->IsNull(2));

    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVectorBatch);
}

TEST(NativeOmniTopNOperatorTest, TestTopNDate32AndDecimal64Column)
{
    using namespace omniruntime::op;

    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;

    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    int64_t data2[dataSize] = {66, 55, 44, 33, 22, 11};

    std::vector<DataTypePtr> types = { Date32Type(DAY), LongType(), Decimal64Type(2, 0) };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2);
    int32_t sortCols[2] = {0, 2};
    int32_t ascendings[2] = {false, true};
    int32_t nullFirsts[2] = {true, true};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNOperator->GetOutput(&outputVecBatch);

    int32_t expectData0[expectedDataSize] = {2, 2, 1, 1, 0};
    int64_t expectData1[expectedDataSize] = {5, 2, 4, 1, 3};
    int64_t expectData2[expectedDataSize] = {11, 44, 22, 55, 33};
    DataTypes expectedTypes(std::vector<DataTypePtr>({ Date32Type(DAY), LongType(), Decimal64Type(2, 1) }));
    VectorBatch *expectVecBatch =
        CreateVectorBatch(sourceTypes, expectedDataSize, expectData0, expectData1, expectData2);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

TEST(NativeOmniTopNOperatorTest, TestTopNDecimal128Column)
{
    using namespace omniruntime::op;

    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;

    // prepare data
    Decimal128 data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    Decimal128 data2[dataSize] = {66, 55, 44, 33, 22, 11};
    std::vector<DataTypePtr> types = { Decimal128Type(2, 1), LongType(), Decimal128Type(2, 1) };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2);
    int32_t sortCols[2] = {0, 2};
    int32_t ascendings[2] = {false, true};
    int32_t nullFirsts[2] = {true, true};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNOperator->GetOutput(&outputVecBatch);

    Decimal128 expectData0[expectedDataSize] = {2, 2, 1, 1, 0};
    int64_t expectData1[expectedDataSize] = {5, 2, 4, 1, 3};
    Decimal128 expectData2[expectedDataSize] = {11, 44, 22, 55, 33};

    DataTypes expectedTypes(std::vector<DataTypePtr>({ Decimal64Type(2, 1), LongType(), Decimal64Type(2, 1) }));
    VectorBatch *expectVecBatch =
        CreateVectorBatch(sourceTypes, expectedDataSize, expectData0, expectData1, expectData2);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

TEST(NativeOmniTopNOperatorTest, TestTopNShortColumn)
{
    using namespace omniruntime::op;

    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;

    // prepare data
    int16_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    int16_t data2[dataSize] = {66, 55, 44, 33, 22, 11};

    std::vector<DataTypePtr> types = { ShortType(), LongType(), ShortType() };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2);
    int32_t sortCols[2] = {0, 2};
    int32_t ascendings[2] = {false, true};
    int32_t nullFirsts[2] = {true, true};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNOperator->GetOutput(&outputVecBatch);

    int16_t expectData0[expectedDataSize] = {2, 2, 1, 1, 0};
    int64_t expectData1[expectedDataSize] = {5, 2, 4, 1, 3};
    int16_t expectData2[expectedDataSize] = {11, 44, 22, 55, 33};
    DataTypes expectedTypes(std::vector<DataTypePtr>({ ShortType(), LongType(), ShortType() }));
    VectorBatch *expectVecBatch =
        CreateVectorBatch(sourceTypes, expectedDataSize, expectData0, expectData1, expectData2);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

TEST(NativeOmniTopNTest, TestTopNDoubleCharColumn)
{
    using namespace omniruntime::op;

    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;
    // prepare data
    std::string data0[dataSize] = {"0", "1", "2", "0", "1", "2"};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    std::string data2[dataSize] = {"6.6", "5.5", "4.4", "3.3", "2.2", "1.1"};

    std::vector<DataTypePtr> types = { VarcharType(1), LongType(), VarcharType(3) };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2);

    int32_t sortCols[2] = {0, 2};
    int32_t ascendings[2] = {false, true};
    int32_t nullFirsts[2] = {true, true};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNOperator->GetOutput(&outputVecBatch);

    std::string expectData0[expectedDataSize]={"2", "2", "1", "1", "0"};
    int64_t expectData1[expectedDataSize] = {5, 2, 4, 1, 3};
    std::string expectData2[expectedDataSize] = {"1.1", "4.4", "2.2", "5.5", "3.3"};
    DataTypes expectedTypes(std::vector<DataTypePtr>({ VarcharType(1), LongType(), VarcharType(3) }));
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectedTypes, expectedDataSize, expectData0, expectData1, expectData2);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

TEST(NativeOmniTopNTest, TestTopNDoubleCharAndBooleanColumn)
{
    using namespace omniruntime::op;

    // construct input data
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 5;
    // prepare data
    std::string data0[dataSize] = {"0", "1", "2", "0", "1", "2"};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    bool data2[dataSize] = {false, false, false, true, true, true};

    std::vector<DataTypePtr> types = { VarcharType(1), LongType(), BooleanType() };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2);

    const int32_t sortColCount = 2;
    int32_t sortCols[sortColCount] = {0, 2};
    int32_t ascendings[sortColCount] = {false, false};
    int32_t nullFirsts[sortColCount] = {true, true};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, sortColCount);

    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNOperator->GetOutput(&outputVecBatch);

    std::string expectData0[expectedDataSize]={"2", "2", "1", "1", "0"};
    int64_t expectData1[expectedDataSize] = {5, 2, 4, 1, 3};
    bool expectData2[expectedDataSize] = {true, false, true, false, true};
    DataTypes expectedTypes(std::vector<DataTypePtr>({ VarcharType(1), LongType(), BooleanType() }));
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectedTypes, expectedDataSize, expectData0, expectData1, expectData2);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

// TopN with ConstVector as non-sort data column
TEST(NativeOmniTopNTest, TestTopNWithConstVectorDataColumn)
{
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 3;
    DataTypes sourceTypes(std::vector<DataTypePtr>({ IntType(), LongType() }));

    // col0: ConstVector (all 5), col1: regular vector
    auto *constCol0 = new ConstVector<int32_t>(5, OMNI_INT, dataSize);
    int64_t data1[dataSize] = {30, 10, 50, 20, 40, 60};
    auto *col1 = new Vector<int64_t>(dataSize);
    for (int32_t i = 0; i < dataSize; i++) {
        col1->SetValue(i, data1[i]);
    }
    auto *vecBatch = new VectorBatch(dataSize);
    vecBatch->Append(constCol0);
    vecBatch->Append(col1);

    int32_t sortCols[] = {1};
    int32_t ascendings[] = {true};
    int32_t nullFirsts[] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);
    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    topNOperator->GetOutput(&outputVecBatch);

    // Top 3 by col1 ascending: (5, 10), (5, 20), (5, 30)
    int32_t expectData0[] = {5, 5, 5};
    int64_t expectData1[] = {10, 20, 30};
    auto expectVecBatch = CreateVectorBatch(sourceTypes, expectedDataSize, expectData0, expectData1);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

// TopN with ConstVector as sort key column
TEST(NativeOmniTopNTest, TestTopNWithConstVectorAsSortKey)
{
    const int32_t dataSize = 6;
    const int32_t expectedDataSize = 3;
    DataTypes sourceTypes(std::vector<DataTypePtr>({ IntType(), LongType() }));

    // col0: ConstVector (all 3, primary sort key), col1: regular vector (secondary sort key)
    auto *constCol0 = new ConstVector<int32_t>(3, OMNI_INT, dataSize);
    int64_t data1[dataSize] = {50, 30, 10, 40, 20, 60};
    auto *col1 = new Vector<int64_t>(dataSize);
    for (int32_t i = 0; i < dataSize; i++) {
        col1->SetValue(i, data1[i]);
    }
    auto *vecBatch = new VectorBatch(dataSize);
    vecBatch->Append(constCol0);
    vecBatch->Append(col1);

    int32_t sortCols[] = {0, 1};
    int32_t ascendings[] = {true, true};
    int32_t nullFirsts[] = {false, false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 2);
    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    topNOperator->GetOutput(&outputVecBatch);

    // All col0=3, sorted by col1 ascending, top 3: (3, 10), (3, 20), (3, 30)
    int32_t expectData0[] = {3, 3, 3};
    int64_t expectData1[] = {10, 20, 30};
    auto expectVecBatch = CreateVectorBatch(sourceTypes, expectedDataSize, expectData0, expectData1);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

// TopN with ConstVector varchar data column
TEST(NativeOmniTopNTest, TestTopNWithConstVectorVarchar)
{
    const int32_t dataSize = 5;
    const int32_t expectedDataSize = 3;
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType() }));

    // col0: ConstVector varchar (all "abc"), col1: regular long vector (sort key)
    std::string_view constStr = "abc";
    auto *constCol0 = new ConstVector<std::string_view>(constStr, OMNI_VARCHAR, dataSize);
    int64_t data1[dataSize] = {50, 20, 40, 10, 30};
    auto *col1 = new Vector<int64_t>(dataSize);
    for (int32_t i = 0; i < dataSize; i++) {
        col1->SetValue(i, data1[i]);
    }
    auto *vecBatch = new VectorBatch(dataSize);
    vecBatch->Append(constCol0);
    vecBatch->Append(col1);

    int32_t sortCols[] = {1};
    int32_t ascendings[] = {true};
    int32_t nullFirsts[] = {false};

    TopNOperatorFactory *topNOperatorFactory =
        new TopNOperatorFactory(sourceTypes, expectedDataSize, 0, sortCols, ascendings, nullFirsts, 1);
    TopNOperator *topNOperator = static_cast<TopNOperator *>(CreateTestOperator(topNOperatorFactory));

    topNOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    topNOperator->GetOutput(&outputVecBatch);

    // Top 3 by col1 ascending: (abc, 10), (abc, 20), (abc, 30)
    std::string expectData0[] = {"abc", "abc", "abc"};
    int64_t expectData1[] = {10, 20, 30};
    auto expectVecBatch = CreateVectorBatch(sourceTypes, expectedDataSize, expectData0, expectData1);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteTopNOperatorFactory(topNOperatorFactory);
}

}
