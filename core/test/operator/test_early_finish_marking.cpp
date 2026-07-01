/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for early-finish operator marking.
 *
 * Verifies that LimitOperator, DistinctLimitOperator, and TableScanOperator
 * override isEarlyFinish() to return true, while a non-early-finish operator
 * (SortOperator) returns the default false.
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include "operator/limit/limit.h"
#include "operator/limit/distinct_limit.h"
#include "operator/tablescan/TableScan.h"
#include "operator/sort/sort.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "util/type_util.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "connectors/Connector.h"
#include "connectors/hive/HiveConnector.h"
#include "compute/task.h"
#include "plannode/planNode.h"

// NOTE: Do NOT include "util/test_util.h" here — it contains
// `using namespace omniruntime::expressions;` which introduces an
// `expressions::Operator` enum that ambiguates `op::Operator` (per coding
// guideline 5.9). This test does not use any TestUtil helper functions.

using namespace omniruntime;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::connector;

// Mock classes for TableScanOperator (reused from tablescan_test.cpp)
class MockTableScanNode : public TableScanNode {
public:
    MockTableScanNode()
        : TableScanNode("test", std::vector<std::shared_ptr<DataType>>(), std::vector<std::string>(),
              std::make_shared<connector::ConnectorTableHandle>("test"),
              std::unordered_map<std::string, std::shared_ptr<connector::ColumnHandle>>())
    {
    }
    MOCK_CONST_METHOD1(CanSpill, bool(const config::QueryConfig &));
    MOCK_CONST_METHOD0(tableHandle, const std::shared_ptr<connector::ConnectorTableHandle>());
    MOCK_CONST_METHOD0(assignments,
        const std::unordered_map<std::string, std::shared_ptr<connector::ColumnHandle>>());
    MOCK_METHOD0(getRowTypePtr, RowTypePtr());
};

// Test 1: LimitOperator reports isEarlyFinish() = true
TEST(EarlyFinishMarkingTest, testLimitOperator_IsEarlyFinish)
{
    auto *factory = LimitOperatorFactory::CreateLimitOperatorFactory(10, 0);
    ASSERT_NE(factory, nullptr);

    auto *op = factory->CreateOperator();
    ASSERT_NE(op, nullptr);

    EXPECT_TRUE(op->isEarlyFinish());

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
}

// Test 2: DistinctLimitOperator reports isEarlyFinish() = true
TEST(EarlyFinishMarkingTest, testDistinctLimitOperator_IsEarlyFinish)
{
    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    DataTypes sourceTypes(types);
    int32_t distinctCols[] = {0, 1, 2};

    auto *factory = DistinctLimitOperatorFactory::CreateDistinctLimitOperatorFactory(
        sourceTypes, distinctCols, sizeof(distinctCols) / sizeof(distinctCols[0]), -1, 10);
    ASSERT_NE(factory, nullptr);

    auto *op = factory->CreateOperator();
    ASSERT_NE(op, nullptr);

    EXPECT_TRUE(op->isEarlyFinish());

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
}

// Test 3: TableScanOperator reports isEarlyFinish() = true
TEST(EarlyFinishMarkingTest, testTableScanOperator_IsEarlyFinish)
{
    MockTableScanNode *mockNode = new MockTableScanNode();
    std::shared_ptr<const TableScanNode> sharedMockNode(mockNode);
    std::shared_ptr<Connector> connector =
        std::make_shared<omniruntime::connector::hive::HiveConnector>("test", nullptr);
    registerConnector(connector);

    TableScanOperator op(sharedMockNode, 1000, common::ReadMode::POSITION_READ, nullptr);

    EXPECT_TRUE(op.isEarlyFinish());

    unregisterConnector(connector->connectorId());
}

// Test 4: SortOperator (non-early-finish) returns false — negative verification
TEST(EarlyFinishMarkingTest, testSortOperator_NotEarlyFinish)
{
    std::vector<DataTypePtr> types = {IntType(), LongType()};
    DataTypes sourceTypes(types);

    int32_t outputCols[] = {0, 1};
    int32_t sortCols[] = {1};
    int32_t ascendings[] = {false};
    int32_t nullFirsts[] = {true};

    auto *factory = SortOperatorFactory::CreateSortOperatorFactory(
        sourceTypes, outputCols, 2, sortCols, ascendings, nullFirsts, 1);
    ASSERT_NE(factory, nullptr);

    auto *op = factory->CreateOperator();
    ASSERT_NE(op, nullptr);

    EXPECT_FALSE(op->isEarlyFinish());

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
}
