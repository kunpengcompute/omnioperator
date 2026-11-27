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

#include "compute/driver.h"
#include "compute/task.h"
#include "compute/local_planner.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "gtest/gtest.h"
#include "test/util/test_util.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace TestUtil;

namespace LimitTest {
class TestBatchIterator : public ColumnarBatchIterator {
public:
    TestBatchIterator(const std::vector<VectorBatch *> &date) : date(date) {}

    ~TestBatchIterator() override = default;

    VectorBatch *Next() override
    {
        if (index < date.size()) {
            return date[index++];
        } else {
            return nullptr;
        }
    }

private:
    std::vector<VectorBatch *> date;
    size_t index = 0;
};

VectorBatch *CreateTestLimitBasicTable1()
{
    int32_t dataSize = 4;
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
    return vecBatch1;
}

VectorBatch *CreateTestLimitBasicExpVecbatch()
{
    int32_t resultDataSize = 3;
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

    VectorBatch *expVecBatch = CreateVectorBatch(sourceTypes, resultDataSize, expData1, expData2, expData3, expData4,
                                                 expData5, expData6, expData7, expData8, expData9, expData10);
    return expVecBatch;
}

// supported data types cover
TEST(PipelineTest, TestLimitBasic)
{
    // construct data
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
    const int32_t dataSize = 4;
    const int32_t limitCount = dataSize - 1;
    const int32_t resultDataSize = dataSize - 1;

    VectorBatch *vecBatch1 = CreateTestLimitBasicTable1();
    std::vector<VectorBatch *> inputVector;
    inputVector.push_back(vecBatch1);

    auto sourceBatchIterator = std::make_unique<TestBatchIterator>(inputVector);
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceBatchIterator));
    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>("value_stream", outTypes, resIterator);

    auto limitNode = std::make_shared<const LimitNode>("limit", 0, limitCount, false, valueStreamNode);
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{limitNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());
    VectorBatch *vectorBatch = nullptr;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!future.valid()) {
            vectorBatch = out;
            break;
        }
        OMNI_CHECK(out == nullptr, "Expected to wait but still got non-null output from Omni task");
        future.wait();
    }

    VectorBatch *expVecBatch1 = CreateTestLimitBasicExpVecbatch();

    EXPECT_TRUE(VecBatchMatch(vectorBatch, expVecBatch1));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(vectorBatch);
}

// test null
TEST(PipelineTest, TestLimitWithNull)
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
    // set null to vector batch to simulate null value
    BaseVector *colVector = vecBatch1->Get(0);
    colVector->SetNull(2);
    colVector->SetNull(3);
    BaseVector *colVector1 = vecBatch1->Get(1);
    colVector1->SetNull(3);
    colVector1->SetNull(4);

    std::vector<VectorBatch *> inputVector;
    inputVector.push_back(vecBatch1);

    auto sourceBatchIterator = std::make_unique<TestBatchIterator>(inputVector);
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceBatchIterator));
    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>("value_stream", outTypes, resIterator);

    auto limitNode = std::make_shared<const LimitNode>("limit", 0, limitCount, false, valueStreamNode);
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{limitNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());
    VectorBatch *vectorBatch = nullptr;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!future.valid()) {
            vectorBatch = out;
            break;
        }
        OMNI_CHECK(out == nullptr, "Expected to wait but still got non-null output from Omni task");
        future.wait();
    }

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

    EXPECT_TRUE(VecBatchMatch(vectorBatch, expVecBatch1));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(vectorBatch);
}

VectorBatch *CreateTestLimitOffsetBasicVecBatch()
{
    const int32_t dataSize = 4;
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
    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5, data6, data7, data8, data9, data10);
    return vecBatch;
}

VectorBatch *CreateTestLimitOffsetBasicExpVecBatch(int32_t resultDataSize)
{
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
    VectorBatch *expVecBatch = CreateVectorBatch(sourceTypes, resultDataSize, expData1, expData2, expData3, expData4,
                                                 expData5, expData6, expData7, expData8, expData9, expData10);
    return expVecBatch;
}

TEST(PipelineTest, TestLimitOffsetBasic)
{
    // construct data
    const int32_t dataSize = 4;
    const int32_t limitCount = dataSize;
    const int32_t offset = 2;
    const int32_t resultDataSize = limitCount - offset;
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

    VectorBatch *vecBatch1 = CreateTestLimitOffsetBasicVecBatch();
    std::vector<VectorBatch *> inputVector;
    inputVector.push_back(vecBatch1);

    auto sourceBatchIterator = std::make_unique<TestBatchIterator>(inputVector);
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceBatchIterator));
    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>("value_stream", outTypes, resIterator);

    auto limitNode = std::make_shared<const LimitNode>("limit", offset, limitCount, false, valueStreamNode);
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{limitNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());
    VectorBatch *vectorBatch = nullptr;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!future.valid()) {
            vectorBatch = out;
            break;
        }
        OMNI_CHECK(out == nullptr, "Expected to wait but still got non-null output from Omni task");
        future.wait();
    }

    VectorBatch *expVecBatch1 = CreateTestLimitOffsetBasicExpVecBatch(resultDataSize);
    EXPECT_TRUE(VecBatchMatch(vectorBatch, expVecBatch1));
    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(vectorBatch);
}
} // namespace LimitTest
