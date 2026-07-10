/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * We modify this part of the code based on Velox to enable native execution of the Omni Spark operator,
 * modify the corresponding code.
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#include "connectors/Connector.h"
#include "connectors/hive/HiveConnector.h"

namespace omniruntime::connector {
namespace {
std::mutex& connectors_mutex()
{
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, std::shared_ptr<ConnectorFactory>>& connectorFactories()
{
    static std::unordered_map<std::string, std::shared_ptr<ConnectorFactory>>
            factories;
    return factories;
}

std::unordered_map<std::string, std::shared_ptr<Connector>>& connectors()
{
    static std::unordered_map<std::string, std::shared_ptr<Connector>> connectors;
    return connectors;
}
} // namespace

bool registerConnectorFactory(std::shared_ptr<ConnectorFactory> factory)
{
    bool ok =
            connectorFactories().insert({factory->connectorName(), factory}).second;
    return true;
}

bool hasConnectorFactory(const std::string& connectorName)
{
    return connectorFactories().count(connectorName) == 1;
}

bool unregisterConnectorFactory(const std::string& connectorName)
{
    auto count = connectorFactories().erase(connectorName);
    return count == 1;
}

std::shared_ptr<ConnectorFactory> getConnectorFactory(
    const std::string& connectorName)
{
    auto it = connectorFactories().find(connectorName);
    return it->second;
}

bool registerConnector(std::shared_ptr<Connector> connector)
{
    std::lock_guard<std::mutex> lock(connectors_mutex());
    bool ok = connectors().insert({connector->connectorId(), connector}).second;
    return true;
}

bool unregisterConnector(const std::string& connectorId)
{
    std::lock_guard<std::mutex> lock(connectors_mutex());
    auto count = connectors().erase(connectorId);
    return count == 1;
}

std::shared_ptr<Connector> getConnector(const std::string& connectorId)
{
    std::lock_guard<std::mutex> lock(connectors_mutex());
    auto& connMap = connectors();
    auto it = connMap.find(connectorId);
    if (it != connMap.end()) {
        return it->second;
    }
    return nullptr;
}

const std::unordered_map<std::string, std::shared_ptr<Connector>>& getAllConnectors()
{
    return connectors();
}
}
