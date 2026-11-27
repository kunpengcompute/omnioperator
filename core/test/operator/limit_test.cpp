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
#include <vector>
#include "gtest/gtest.h"
#include "vector/vector_helper.h"
#include "operator/limit/limit.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace std;
using namespace omniruntime::TestUtil;

namespace LimitTest {
// supported data types cover
TEST(NativeOmniLimitOperator, TestLimitBasic)
{
    // construct data
    const int32_t dataSize = 4;
    const int32_t limitCount = dataSize - 1;
    const int32_t resultDataSize = dataSize - 1;

    // table1
    int32_t data1[dataSize] = {0, 1, 2, 0};
    double data2[dataSize] = {0.1, 1.1, 2.1, 0.1};
    std::string data3[dataSize] = {"abc", "hello", "world", "abc"};
    int64_t data4[dataSize] = {10L, 100L, 1000L, 10L};
    Decimal128 data5[dataSize] = {111111, 222222, 333333, 111111};
    int32_t data6[dataSize] = {0, 1, 2, 0};
    int64_t data7[dataSize] = {10L, 100L, 1000L, 10L};
    bool data8[dataSize] = {true, false, false, true};
    std::string data9[dataSize] = {"123", "456", "789", "012"};
    int16_t data10[dataSize] = {0, 1, 2, 3};

    std::vector<DataTypePtr> types = {IntType(),
                                      DoubleType(),
                                      VarcharType(10),
                                      LongType(),
                                      Decimal128Type(10, 2),
                                      Date32Type(),
                                      Decimal64DataType::Instance(),
                                      BooleanType(),
                                      CharDataType::Instance(),
                                      ShortType()};
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch1 =
        CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5, data6, data7, data8, data9, data10);

    LimitOperatorFactory *operatorFactory = LimitOperatorFactory::CreateLimitOperatorFactory(limitCount, 0);
    LimitOperator *limitOperator = dynamic_cast<LimitOperator *>(CreateTestOperator(operatorFactory));
    limitOperator->AddInput(vecBatch1);
    VectorBatch *outputVecBatch;
    limitOperator->GetOutput(&outputVecBatch);

    int32_t expData1[resultDataSize] = {0, 1, 2};
    double expData2[resultDataSize] = {0.1, 1.1, 2.1};
    std::string expData3[resultDataSize] = {"abc", "hello", "world"};
    int64_t expData4[resultDataSize] = {10L, 100L, 1000L};
    Decimal128 expData5[resultDataSize] = {111111, 222222, 333333};
    int32_t expData6[resultDataSize] = {0, 1, 2};
    int64_t expData7[resultDataSize] = {10L, 100L, 1000L};
    bool expData8[resultDataSize] = {true, false, false};
    std::string expData9[resultDataSize] = {"123", "456", "789"};
    int16_t expData10[resultDataSize] = {0, 1, 2};

    VectorBatch *expVecBatch1 = CreateVectorBatch(sourceTypes, resultDataSize, expData1, expData2, expData3, expData4,
                                                  expData5, expData6, expData7, expData8, expData9, expData10);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expVecBatch1));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(limitOperator);
    delete operatorFactory;
}

// call input several times
TEST(NativeOmniLimitOperator, TestLimitMultiInput)
{
    // construct data
    const int32_t dataSize = 3;
    const int32_t limitCount = 40;

    // input vecBatch1
    int32_t data01[dataSize] = {0, 1, 2};
    double data02[dataSize] = {6.6, 5.5, 4.4};

    std::vector<DataTypePtr> types = {IntType(), DoubleType()};
    DataTypes sourceTypes(types);

    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize, data01, data02);
    LimitOperatorFactory *operatorFactory = LimitOperatorFactory::CreateLimitOperatorFactory(limitCount, 0);
    LimitOperator *limitOperator = dynamic_cast<LimitOperator *>(CreateTestOperator(operatorFactory));
    VectorBatch *outputVecBatch1;
    limitOperator->AddInput(vecBatch1);
    limitOperator->GetOutput(&outputVecBatch1);

    // input vecBatch2
    int32_t data11[dataSize] = {3, 4, 5};
    double data12[dataSize] = {3.3, 2.2, 1.1};

    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes, dataSize, data11, data12);
    limitOperator->AddInput(vecBatch2);
    VectorBatch *outputVecBatch2;
    limitOperator->GetOutput(&outputVecBatch2);

    int32_t expData01[dataSize] = {0, 1, 2};
    double expData02[dataSize] = {6.6, 5.5, 4.4};
    VectorBatch *expVecBatch1 = CreateVectorBatch(sourceTypes, (dataSize), expData01, expData02);

    int32_t expData11[dataSize] = {3, 4, 5};
    double expData12[dataSize] = {3.3, 2.2, 1.1};
    VectorBatch *expVecBatch2 = CreateVectorBatch(sourceTypes, (dataSize), expData11, expData12);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch1, expVecBatch1));
    EXPECT_TRUE(VecBatchMatch(outputVecBatch2, expVecBatch2));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(expVecBatch2);
    VectorHelper::FreeVecBatch(outputVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch2);
    omniruntime::op::Operator::DeleteOperator(limitOperator);
    delete operatorFactory;
}

// test null
TEST(NativeOmniLimitOperator, TestLimitWithNull)
{
    // construct data
    const int32_t dataSize = 6;
    const int32_t limitCount = 5;
    // table1
    int32_t data1[dataSize] = {0, 1, 2, 0, 1, 2};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};

    std::vector<DataTypePtr> types = {IntType(), DoubleType()};
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    LimitOperatorFactory *operatorFactory = LimitOperatorFactory::CreateLimitOperatorFactory(limitCount, 0);
    LimitOperator *limitOperator = dynamic_cast<LimitOperator *>(CreateTestOperator(operatorFactory));

    // set null to vector batch to simulate null value
    BaseVector *colVector = vecBatch1->Get(0);
    colVector->SetNull(2);
    colVector->SetNull(3);
    BaseVector *colVector1 = vecBatch1->Get(1);
    colVector1->SetNull(3);
    colVector1->SetNull(4);

    limitOperator->AddInput(vecBatch1);
    VectorBatch *outputVecBatch;
    limitOperator->GetOutput(&outputVecBatch);

    int32_t expData1[dataSize] = {0, 1, 2, 0, 1};          // expData1[2],expData1[3] simulate to null
    double expData2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2}; // expData2[3],expData2[4] simulate to null
    VectorBatch *expVecBatch1 = CreateVectorBatch(sourceTypes, limitCount, expData1, expData2);

    // set null to vector batch to simulate null value
    BaseVector *colVector00 = expVecBatch1->Get(0);
    colVector00->SetNull(2);
    colVector00->SetNull(3);
    BaseVector *colVector01 = expVecBatch1->Get(1);
    colVector01->SetNull(3);
    colVector01->SetNull(4);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expVecBatch1));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(limitOperator);
    delete operatorFactory;
}

TEST(NativeOmniLimitOperator, TestLimitOffsetBasic)
{
    // construct data
    const int32_t dataSize = 4;
    const int32_t limitCount = dataSize;
    const int32_t offset = 2;
    const int32_t resultDataSize = limitCount - offset;
    // table1
    int32_t data1[dataSize] = {0, 1, 2, 0};
    double data2[dataSize] = {0.1, 1.1, 2.1, 0.1};
    std::string data3[dataSize] = {"abc", "hello", "world", "abc"};
    int64_t data4[dataSize] = {10L, 100L, 1000L, 10L};
    Decimal128 data5[dataSize] = {111111, 222222, 333333, 111111};
    int32_t data6[dataSize] = {0, 1, 2, 0};
    int64_t data7[dataSize] = {10L, 100L, 1000L, 10L};
    bool data8[dataSize] = {true, false, false, true};
    std::string data9[dataSize] = {"123", "456", "789", "012"};
    int16_t data10[dataSize] = {0, 1, 2, 3};
    std::vector<DataTypePtr> types = {IntType(),
                                      DoubleType(),
                                      VarcharType(10),
                                      LongType(),
                                      Decimal128Type(10, 2),
                                      Date32Type(),
                                      Decimal64DataType::Instance(),
                                      BooleanType(),
                                      CharDataType::Instance(),
                                      ShortType()};
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch1 =
        CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5, data6, data7, data8, data9, data10);
    LimitOperatorFactory *operatorFactory = LimitOperatorFactory::CreateLimitOperatorFactory(limitCount, offset);
    LimitOperator *limitOperator = dynamic_cast<LimitOperator *>(CreateTestOperator(operatorFactory));
    limitOperator->AddInput(vecBatch1);
    VectorBatch *outputVecBatch;
    limitOperator->GetOutput(&outputVecBatch);
    int32_t expData1[resultDataSize] = {2, 0};
    double expData2[resultDataSize] = {2.1, 0.1};
    std::string expData3[resultDataSize] = {"world", "abc"};
    int64_t expData4[resultDataSize] = {1000L, 10L};
    Decimal128 expData5[resultDataSize] = {333333, 111111};
    int32_t expData6[resultDataSize] = {2, 0};
    int64_t expData7[resultDataSize] = {1000L, 10L};
    bool expData8[resultDataSize] = {false, true};
    std::string expData9[resultDataSize] = {"789", "012"};
    int16_t expData10[resultDataSize] = {2, 3};
    VectorBatch *expVecBatch1 = CreateVectorBatch(sourceTypes, resultDataSize, expData1, expData2, expData3, expData4,
                                                  expData5, expData6, expData7, expData8, expData9, expData10);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expVecBatch1));
    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(limitOperator);
    delete operatorFactory;
}

TEST(NativeOmniLimitOperator, TestLimitOffsetMultiInput)
{
    // construct data
    const int32_t dataSize = 3;
    const int32_t limitCount = 40;
    const int32_t offset = 1;
    const int32_t resultDataSize = dataSize - offset;

    // input vecBatch1
    int32_t data01[dataSize] = {0, 1, 2};
    double data02[dataSize] = {6.6, 5.5, 4.4};

    std::vector<DataTypePtr> types = {IntType(), DoubleType()};
    DataTypes sourceTypes(types);

    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize, data01, data02);
    LimitOperatorFactory *operatorFactory = LimitOperatorFactory::CreateLimitOperatorFactory(limitCount, offset);
    LimitOperator *limitOperator = dynamic_cast<LimitOperator *>(CreateTestOperator(operatorFactory));
    VectorBatch *outputVecBatch1;
    limitOperator->AddInput(vecBatch1);
    limitOperator->GetOutput(&outputVecBatch1);

    // input vecBatch2
    int32_t data11[dataSize] = {3, 4, 5};
    double data12[dataSize] = {3.3, 2.2, 1.1};

    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes, dataSize, data11, data12);
    limitOperator->AddInput(vecBatch2);
    VectorBatch *outputVecBatch2;
    limitOperator->GetOutput(&outputVecBatch2);

    int32_t expData01[resultDataSize] = {1, 2};
    double expData02[resultDataSize] = {5.5, 4.4};
    VectorBatch *expVecBatch1 = CreateVectorBatch(sourceTypes, (resultDataSize), expData01, expData02);

    int32_t expData11[dataSize] = {3, 4, 5};
    double expData12[dataSize] = {3.3, 2.2, 1.1};
    VectorBatch *expVecBatch2 = CreateVectorBatch(sourceTypes, (dataSize), expData11, expData12);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch1, expVecBatch1));
    EXPECT_TRUE(VecBatchMatch(outputVecBatch2, expVecBatch2));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(expVecBatch2);
    VectorHelper::FreeVecBatch(outputVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch2);
    omniruntime::op::Operator::DeleteOperator(limitOperator);
    delete operatorFactory;
}
} // namespace LimitTest
