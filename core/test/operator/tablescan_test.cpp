/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <deque>
#include "operator/tablescan/TableScan.h"
#include "vector/vector_batch.h"
#include "connectors/Split.h"
#include "plannode/planNode.h"
#include "compute/task.h"
#include "codegen/time_util.h"
#include "type/data_type.h"
#include "connectors/Connector.h"
#include "connectors/hive/HiveConnector.h"

using namespace omniruntime;
using namespace omniruntime::op;
using namespace omniruntime::connector;
using namespace testing;

// Mock classes for testing
class MockTableScanNode : public TableScanNode {
public:
    MockTableScanNode() : TableScanNode("test", std::vector<std::shared_ptr<DataType>>(), std::vector<std::string>(), std::make_shared<connector::ConnectorTableHandle>("test"), std::unordered_map<std::string, std::shared_ptr<connector::ColumnHandle>>()) {}
    MOCK_CONST_METHOD1(CanSpill, bool(const config::QueryConfig&));
    MOCK_CONST_METHOD0(tableHandle, const std::shared_ptr <connector::ConnectorTableHandle>());
    MOCK_CONST_METHOD0(assignments, const std::unordered_map<std::string, std::shared_ptr<connector::ColumnHandle>>());
    MOCK_METHOD0(getRowTypePtr, RowTypePtr());
};

class MockConnector : public Connector {
public:
    explicit MockConnector(const std::string& id) : Connector(id) {}
    MOCK_METHOD4(createDataSource, std::unique_ptr<connector::DataSource>(
            const type::RowTypePtr&,
            const std::shared_ptr<ConnectorTableHandle>&,
            const std::unordered_map<std::string, std::shared_ptr<connector::ColumnHandle>>&,
            omniruntime::mem::OmniTaskMemoryPool*));

    // Mock supportsIndexLookup function
    MOCK_CONST_METHOD0(supportsIndexLookup, bool());
};

class MockDataSource : public connector::DataSource {
public:
    MockDataSource() : connector::DataSource() {}
    MOCK_METHOD2(addSplit, void(std::shared_ptr<ConnectorSplit>, uint64_t));
    MOCK_METHOD1(next, std::optional<omniruntime::vec::VectorBatch*>(uint64_t));
    MOCK_METHOD0(cancel, void());
};

TEST(TableScanOperatorTest, AddInput) {
// Setup
MockTableScanNode* mockNode = new MockTableScanNode();
std::shared_ptr<const TableScanNode> sharedMockNode(mockNode);
std::shared_ptr<Connector> connector1 = std::make_shared<omniruntime::connector::hive::HiveConnector>("test", nullptr);
registerConnector(connector1);

TableScanOperator op(nullptr, sharedMockNode, 1000, nullptr);

// Test
EXPECT_EQ(op.AddInput(nullptr), 0);
unregisterConnector(connector1->connectorId());
}

TEST(TableScanOperatorTest, GetOutput) {
// Setup mock objects
MockTableScanNode* mockNode = new MockTableScanNode();
std::shared_ptr<const TableScanNode> sharedMockNode(mockNode);
auto mockSplitsStore = std::make_shared<SplitsStore >();
auto split = Split(std::make_shared<omniruntime::connector::ConnectorSplit>("test-hive"));
mockSplitsStore->splits.push_back(std::move(split));
OperatorConfig config;

MockConnector* connector1 = new MockConnector("test");
auto mockDataSource = std::make_unique<MockDataSource>();
// 保存一个指向 mockDataSource 的原始指针，以便后续设置其 next 方法的行为
MockDataSource* rawMockDataSourcePtr = mockDataSource.get();

EXPECT_CALL(*connector1, createDataSource(_, _, _, _)).WillRepeatedly(Return(ByMove(std::move(mockDataSource))));
EXPECT_CALL(*rawMockDataSourcePtr, next(_)).WillOnce(Return(std::make_optional<omniruntime::vec::VectorBatch*>(new omniruntime::vec::VectorBatch(1))));

std::shared_ptr<Connector> sharedConnector(connector1);
registerConnector(sharedConnector);

TableScanOperator op(nullptr, sharedMockNode, 1000, mockSplitsStore);
VectorBatch* output = nullptr;

// Test
EXPECT_EQ(op.GetOutput(&output), 0);
EXPECT_NE(output, nullptr);
unregisterConnector(sharedConnector->connectorId());
delete output;
}

TEST(TableScanOperatorTest, Close) {
// Setup
MockTableScanNode* mockNode = new MockTableScanNode();
std::shared_ptr<const TableScanNode> sharedMockNode(mockNode);
std::shared_ptr<Connector> connector1 = std::make_shared<omniruntime::connector::hive::HiveConnector>("test", nullptr);
registerConnector(connector1);

TableScanOperator op(nullptr, sharedMockNode, 1000, nullptr);

// Test
EXPECT_EQ(op.Close(), OMNI_STATUS_NORMAL);
unregisterConnector(connector1->connectorId());
}

TEST(TableScanOperatorTest, IsFinished) {
MockTableScanNode* mockNode = new MockTableScanNode();
std::shared_ptr<const TableScanNode> sharedMockNode(mockNode);
std::shared_ptr<Connector> connector1 = std::make_shared<omniruntime::connector::hive::HiveConnector>("test", nullptr);
registerConnector(connector1);

TableScanOperator op(nullptr, sharedMockNode, 1000, nullptr);

EXPECT_FALSE(op.isFinished());
unregisterConnector(connector1->connectorId());
}