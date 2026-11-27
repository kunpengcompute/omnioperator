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

#include "HiveDataSource.h"
#include "HiveConnector.h"
#include "HiveConfig.h"

#include <memory>
#include <random>

namespace omniruntime::connector::hive {

namespace {
std::vector<std::unique_ptr<HiveConnectorMetadataFactory>>&
hiveConnectorMetadataFactories() {
  static std::vector<std::unique_ptr<HiveConnectorMetadataFactory>> factories;
  return factories;
}
} // namespace

HiveConnector::HiveConnector(
        const std::string& id,
        std::shared_ptr<const config::ConfigBase> config)
        : Connector(id),
          hiveConfig_(std::make_shared<HiveConfig>(config))
{
    for (auto& factory : hiveConnectorMetadataFactories()) {
        metadata_ = factory->create(this);
        if (metadata_ != nullptr) {
            break;
        }
    }
}

std::unique_ptr<DataSource> HiveConnector::createDataSource(
        const RowTypePtr& outputType,
        const std::shared_ptr<ConnectorTableHandle>& tableHandle,
        const std::unordered_map<
        std::string,
        std::shared_ptr<connector::ColumnHandle>>& columnHandles,
        omniruntime::mem::OmniTaskMemoryPool *taskPool)
{
    return std::make_unique<HiveDataSource>(
            outputType,
            tableHandle,
            columnHandles,
            hiveConfig_,
            taskPool);
}

bool registerHiveConnectorMetadataFactory(
    std::unique_ptr<HiveConnectorMetadataFactory> factory)
{
    hiveConnectorMetadataFactories().push_back(std::move(factory));
    return true;
}

} // namespace connector::hive
