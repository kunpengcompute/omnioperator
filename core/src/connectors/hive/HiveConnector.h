/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#pragma once

#include "connectors/Connector.h"
#include "HiveConfig.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "util/config/Config.h"
#include "plannode/planNode.h"

namespace omniruntime::connector::hive {

class HiveConnector : public Connector {
public:
    HiveConnector(
            const std::string &id,
            std::shared_ptr<const config::ConfigBase> config);

    const std::shared_ptr<const config::ConfigBase> &connectorConfig()
    const
    {
        return hiveConfig_->config();
    }

    ~HiveConnector() {

    }

    bool canAddDynamicFilter() const override
    {
        return true;
    }

    std::unique_ptr <DataSource> createDataSource(
            const RowTypePtr &outputType,
            const std::shared_ptr <ConnectorTableHandle> &tableHandle,
            const std::unordered_map <
            std::string,
            std::shared_ptr<connector::ColumnHandle>> &columnHandles,
            omniruntime::mem::OmniTaskMemoryPool *taskPool = nullptr) override;

    bool supportsSplitPreload() override
    {
        return true;
    }

protected:
    const std::shared_ptr <HiveConfig> hiveConfig_;
    std::shared_ptr <ConnectorMetadata> metadata_;
};

class HiveConnectorFactory : public ConnectorFactory {
public:
    static constexpr const char* kHiveConnectorName = "hive";

    HiveConnectorFactory() : ConnectorFactory(kHiveConnectorName) {}

    explicit HiveConnectorFactory(const char* connectorName)
            : ConnectorFactory(connectorName) {}

    std::shared_ptr<Connector> newConnector(
            const std::string& id,
            std::shared_ptr<const config::ConfigBase> config) override
    {
        return std::make_shared<HiveConnector>(id, config);
    }
};

class HivePartitionFunctionSpec : public PartitionFunctionSpec {
public:
    HivePartitionFunctionSpec(
            std::vector<type::column_index_t> channels,
            std::vector<vec::VectorPtr> constValues)
            : channels_(std::move(channels)),
              constValues_(std::move(constValues)) {}

private:
    const std::vector<column_index_t> channels_;
    const std::vector<vec::VectorPtr> constValues_;
};

void registerHivePartitionFunctionSerDe();

/// Hook for connecting metadata functions to a HiveConnector. Each registered
/// factory is called after initializing a HiveConnector until one of these
/// returns a ConnectorMetadata instance.
class HiveConnectorMetadataFactory {
 public:
  virtual ~HiveConnectorMetadataFactory() = default;

  /// Returns a ConnectorMetadata to complete'hiveConnector' if 'this'
  /// recognizes a data source, e.g. local file system or remote metadata
  /// service associated to configs in 'hiveConnector'.
  virtual std::shared_ptr<ConnectorMetadata> create(
      HiveConnector* connector) = 0;
};

bool registerHiveConnectorMetadataFactory(
    std::unique_ptr<HiveConnectorMetadataFactory>);

} // namespace connector::hive
