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

namespace UnionTest {
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

VectorBatch *CreateTestUnionByThreeColumnVecBatch1()
{
    const int32_t dataSize = 6;
    int32_t data1[dataSize] = {0, 1, 2, 0, 1, 2};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {6, 5, 4, 3, 2, 1};

    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    DataTypes sourceTypes(types);

    return CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);
}

VectorBatch *CreateTestUnionByThreeColumnVecBatch2()
{
    const int32_t dataSize = 6;
    int32_t data1[dataSize] = {10, 11, 12, 10, 11, 12};
    double data2[dataSize] = {16.6, 15.5, 14.4, 13.3, 12.2, 11.1};
    int16_t data3[dataSize] = {16, 15, 14, 13, 12, 11};

    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    DataTypes sourceTypes(types);

    return CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);
}

VectorBatch *CreateTestUnionByThreeColumnOutputVecBatch1()
{
    const int32_t dataSize = 6;
    int32_t expData1[dataSize] = {0, 1, 2, 0, 1, 2};
    double expData2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t expData3[dataSize] = {6, 5, 4, 3, 2, 1};

    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    DataTypes sourceTypes(types);

    return CreateVectorBatch(sourceTypes, dataSize, expData1, expData2, expData3);
}

VectorBatch *CreateTestUnionByThreeColumnOutputVecBatch2()
{
    const int32_t dataSize = 6;
    int32_t expData1[dataSize] = {10, 11, 12, 10, 11, 12};
    double expData2[dataSize] = {16.6, 15.5, 14.4, 13.3, 12.2, 11.1};
    int16_t expData3[dataSize] = {16, 15, 14, 13, 12, 11};

    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    DataTypes sourceTypes(types);

    return CreateVectorBatch(sourceTypes, dataSize, expData1, expData2, expData3);
}

TEST(PipelineTest, TestUnionByThreeColumn)
{
    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    VectorBatch *vecBatch1 = CreateTestUnionByThreeColumnVecBatch1();
    VectorBatch *vecBatch2 = CreateTestUnionByThreeColumnVecBatch1();
    std::vector<VectorBatch *> inputVector1;
    std::vector<VectorBatch *> inputVector2;
    inputVector1.push_back(vecBatch1);
    inputVector2.push_back(vecBatch2);

    auto sourceBatchIterator1 = std::make_unique<TestBatchIterator>(inputVector1);
    auto resIterator1 = std::make_shared<ResultIterator>(std::move(sourceBatchIterator1));
    auto outTypes1 = std::make_shared<DataTypes>(types);
    auto valueStreamNode1 = std::make_shared<const ValueStreamNode>("value_stream", outTypes1, resIterator1);

    auto sourceBatchIterator2 = std::make_unique<TestBatchIterator>(inputVector2);
    auto resIterator2 = std::make_shared<ResultIterator>(std::move(sourceBatchIterator2));
    auto outTypes2 = std::make_shared<DataTypes>(types);
    auto valueStreamNode2 = std::make_shared<const ValueStreamNode>("value_stream", outTypes2, resIterator2);

    auto sources = std::vector<PlanNodePtr>{valueStreamNode1, valueStreamNode2};
    auto unionNode = std::make_shared<const UnionNode>("union", sources, false);
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{unionNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());
    VectorBatch *vectorBatch1 = nullptr;
    VectorBatch *vectorBatch2 = nullptr;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out1 = task->Next(&future);
        auto out2 = task->Next(&future);
        if (!future.valid()) {
            vectorBatch1 = out1;
            vectorBatch2 = out2;
            break;
        }
        OMNI_CHECK(out1 == nullptr, "Expected to wait but still got non-null output from Omni task");
        OMNI_CHECK(out2 == nullptr, "Expected to wait but still got non-null output from Omni task");
        future.wait();
    }

    VectorBatch *expVecBatch1 = CreateTestUnionByThreeColumnOutputVecBatch1();
    VectorBatch *expVecBatch2 = CreateTestUnionByThreeColumnOutputVecBatch1();

    EXPECT_TRUE(VecBatchMatch(vectorBatch2, expVecBatch1));
    EXPECT_TRUE(VecBatchMatch(vectorBatch1, expVecBatch2));
    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(expVecBatch2);
    VectorHelper::FreeVecBatch(vectorBatch1);
    VectorHelper::FreeVecBatch(vectorBatch2);
}

} // namespace UnionTest
