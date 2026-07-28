/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Dynamic UDAF registry for Gluten-loaded aggregate libraries.
 */

#include "operator/aggregation/udaf/udaf_registry.h"

#include "util/omni_exception.h"

namespace omniruntime {
namespace op {

UdafRegistry &UdafRegistry::getInstance()
{
    static UdafRegistry instance;
    return instance;
}

void UdafRegistry::registerUdaf(const std::string &name, UdafAggregatorCreator creator)
{
    creators_[name] = std::move(creator);
}

bool UdafRegistry::contains(const std::string &name) const
{
    return creators_.find(name) != creators_.end();
}

std::unique_ptr<Aggregator> UdafRegistry::create(
    const std::string &name,
    const type::DataTypes &inputTypes,
    const type::DataTypes &outputTypes,
    std::vector<int32_t> &channels,
    bool inputRaw,
    bool outputPartial,
    bool isOverflowAsNull) const
{
    auto it = creators_.find(name);
    if (it == creators_.end()) {
        throw omniruntime::exception::OmniException(
            "UNSUPPORTED_ERROR", "UDAF not registered in Omni runtime: " + name);
    }
    return it->second(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
}

std::unordered_set<std::string> UdafRegistry::getRegisteredNames() const
{
    std::unordered_set<std::string> names;
    for (const auto &entry : creators_) {
        names.insert(entry.first);
    }
    return names;
}

} // namespace op
} // namespace omniruntime
