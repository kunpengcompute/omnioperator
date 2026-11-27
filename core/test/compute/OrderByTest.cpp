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
#include "gtest/gtest.h"
#include "test/util/test_util.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace TestUtil;

namespace orderbyTest {
class TestBatchIterator final : public ColumnarBatchIterator {
public:
    explicit TestBatchIterator(const std::vector<VectorBatch *> &date) : date(date) {}

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

TEST(DriverTest, TestOrderBy)
{
    std::vector<DataTypePtr> types = {LongType()};
    DataTypes sourceTypes(types);
    const int64_t dataSize1 = 6;
    int64_t data1[dataSize1] = {6, 4, 7, 3, 2, 1};
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize1, data1);
    std::vector<VectorBatch *> inputVector;
    inputVector.push_back(vecBatch1);

    auto sourceBatchIterator = std::make_unique<TestBatchIterator>(inputVector);
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceBatchIterator));
    auto outTypes = std::make_shared<DataTypes>(std::vector<DataTypePtr>({LongType()}));
    auto valueStreamNode = std::make_shared<const ValueStreamNode>("value_stream", outTypes, resIterator);

    std::vector<int32_t> c = {0};
    auto expr = new FieldExpr(0, LongType());
    std::vector<Expr *> expressions = {static_cast<Expr *>(expr)};
    auto orderbyNode = std::make_shared<const OrderByNode>("order_by", c, c, c, valueStreamNode, expressions);

    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{orderbyNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());
    VectorBatch *vectorBatch = nullptr;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!future.valid()) {
            // Not need to wait. Break.
            vectorBatch = out;
            break;
        }
        // Omni suggested to wait. This might be because another thread (e.g., background io thread)
        // is spilling the task.
        OMNI_CHECK(out == nullptr, "Expected to wait but still got non-null output from Omni task");
        future.wait();
    }
    int64_t expect[dataSize1] = {7, 6, 4, 3, 2, 1};
    VectorBatch *vecBatchExpect = CreateVectorBatch(sourceTypes, dataSize1, expect);
    ASSERT_TRUE(VecBatchMatch(vecBatchExpect, vectorBatch));
    VectorHelper::FreeVecBatch(vecBatchExpect);
    VectorHelper::FreeVecBatch(vectorBatch);
    delete expr;
}

TEST(DriverTest, TestGroupingOperator)
{
    std::vector<DataTypePtr> types = {IntType(), IntType(), IntType(), IntType()};
    DataTypes sourceTypes(types);
    const int64_t dataSize1 = 4;
    int32_t data1[dataSize1] = {1, 2, 3, 4};
    int32_t data2[dataSize1] = {1, 2, 3, 5};
    int32_t data3[dataSize1] = {1, 2, 2, 5};
    int32_t data4[dataSize1] = {2, 2, 2, 5};
    int64_t data5[dataSize1] = {1, 1, 2, 5};
    int64_t data6[dataSize1] = {1, 1, 2, 5};
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize1, data1, data2, data3, data4, data5, data6);
    std::vector<VectorBatch *> inputVector;
    inputVector.push_back(vecBatch1);

    auto sourceBatchIterator = std::make_unique<TestBatchIterator>(inputVector);
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceBatchIterator));
    auto outTypes =
        std::make_shared<DataTypes>(std::vector<DataTypePtr>({IntType(), IntType(), IntType(), IntType(), LongType()}));
    auto valueStreamNode = std::make_shared<const ValueStreamNode>("value_stream", outTypes, resIterator);
    std::vector<ExprPtr> projectSet1 = {new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
                                        new FieldExpr(2, IntType()), new FieldExpr(3, IntType()),
                                        new LiteralExpr(0, LongType())};
    std::vector<ExprPtr> projectSet2 = {new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
                                        new FieldExpr(2, IntType()), new LiteralExpr(0, IntType(), true),
                                        new LiteralExpr(0, LongType())};
    std::vector<ExprPtr> projectSet3 = {new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
                                        new LiteralExpr(0, IntType(), true), new LiteralExpr(0, IntType(), true),
                                        new LiteralExpr(0, LongType())};
    std::vector<ExprPtr> projectSet4 = {new FieldExpr(0, IntType()), new LiteralExpr(0, IntType(), true),
                                        new LiteralExpr(0, IntType(), true), new LiteralExpr(0, IntType(), true),
                                        new LiteralExpr(0, LongType())};

    std::vector<std::vector<ExprPtr>> projectSetExprs = {projectSet1, projectSet2, projectSet3, projectSet4};
    auto expandNode = std::make_shared<ExpandNode>("ExpandNode", std::move(projectSetExprs), valueStreamNode);

    std::vector<int32_t> c = {0};
    uint32_t groupByNum = 4;
    auto sourceDataTypes =
        std::make_shared<DataTypes>(std::vector<DataTypePtr>({IntType(), IntType(), IntType(), IntType(), LongType()}));
    std::vector<ExprPtr> groupingExprs = {new FieldExpr(1, IntType()), new FieldExpr(2, IntType()),
                                          new FieldExpr(3, IntType()), new FieldExpr(4, LongType())};
    std::vector<std::vector<ExprPtr>> aggsKeys = {{new FieldExpr(0, IntType())}};
    std::vector<ExprPtr> aggFilters = {};
    std::vector<DataTypes> outPutDataTypes = {DataTypes(std::vector<DataTypePtr>({LongType()}))};
    std::vector<uint32_t> aggFuncTypes = {0};
    std::vector<uint32_t> maskColumns = {4294967295};
    std::vector<bool> inputRaws = {true};
    std::vector<bool> outputPartial = {true};
    bool isStatisticalAggregate = false;
    auto aggregationNode = std::make_shared<AggregationNode>(
        "AggregationNode", groupingExprs, groupByNum, aggsKeys, sourceDataTypes, outPutDataTypes, aggFuncTypes,
        aggFilters, maskColumns, inputRaws, outputPartial, isStatisticalAggregate, sourceDataTypes, valueStreamNode);

    auto groupingNode = std::make_shared<GroupingNode>("GroupingNode", expandNode, aggregationNode);

    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{groupingNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());
    int32_t outputRow = 0;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!out) {
            break;
        }
        outputRow += out->GetRowCount();
        VectorHelper::FreeVecBatch(out);
    }
    ASSERT_EQ(outputRow, 13);
    for (auto expr : groupingExprs) {
        delete expr;
    }
    delete aggsKeys[0][0];
}
} // namespace orderbyTest
