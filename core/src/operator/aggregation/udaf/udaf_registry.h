/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Dynamic UDAF registry for Gluten-loaded aggregate libraries.
 */
#ifndef OMNI_RUNTIME_UDAF_REGISTRY_H
#define OMNI_RUNTIME_UDAF_REGISTRY_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "operator/aggregation/aggregator/aggregator.h"
#include "type/data_types.h"

namespace omniruntime {
namespace op {

using UdafAggregatorCreator = std::function<std::unique_ptr<Aggregator>(
    const type::DataTypes &inputTypes,
    const type::DataTypes &outputTypes,
    std::vector<int32_t> &channels,
    bool inputRaw,
    bool outputPartial,
    bool isOverflowAsNull)>;

// Registry for dynamic UDAF creators loaded from Gluten UDAF shared libraries.
class UdafRegistry {
public:
    static UdafRegistry &getInstance();

    void registerUdaf(const std::string &name, UdafAggregatorCreator creator);

    bool contains(const std::string &name) const;

    std::unique_ptr<Aggregator> create(
        const std::string &name,
        const type::DataTypes &inputTypes,
        const type::DataTypes &outputTypes,
        std::vector<int32_t> &channels,
        bool inputRaw,
        bool outputPartial,
        bool isOverflowAsNull) const;

    std::unordered_set<std::string> getRegisteredNames() const;

private:
    UdafRegistry() = default;

    std::unordered_map<std::string, UdafAggregatorCreator> creators_;
};

} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_UDAF_REGISTRY_H
