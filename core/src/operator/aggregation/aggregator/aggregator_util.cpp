/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: aggregator tool class
 */

#include "aggregator_util.h"

namespace omniruntime {
namespace op {
using namespace type;
//   DataTypes(LONG, INT) => vector(DataTypes(LONG),DataTypes(INT))
std::vector<DataTypes> AggregatorUtil::WrapWithVector(const DataTypes &value)
{
    std::vector<DataTypes> retVector;
    retVector.reserve(value.GetSize());
    for (int i = 0; i < value.GetSize(); ++i) {
        std::vector<DataTypePtr> vector{value.GetType(i)};
        retVector.emplace_back(DataTypes(vector));
    }
    return retVector;
}

std::vector<std::vector<uint32_t>> AggregatorUtil::WrapWithVector(const std::vector<uint32_t> &values)
{
    std::vector<std::vector<uint32_t>> twoLayerVectors;
    twoLayerVectors.reserve(values.size());
    for (auto value : values) {
        twoLayerVectors.push_back({value});
    }
    return twoLayerVectors;
}

// vector(1,2,3) => vector(vector(1),vector(2),vector(3))
std::vector<std::vector<int32_t>> AggregatorUtil::WrapWithVector(std::vector<int32_t> &values)
{
    std::vector<std::vector<int32_t>> twoLayerVectors;
    twoLayerVectors.reserve(values.size());
    for (auto value : values) {
        twoLayerVectors.push_back({value});
    }
    return twoLayerVectors;
}

std::unique_ptr<DataTypes> AggregatorUtil::WrapWithDataTypes(const DataTypePtr &value)
{
    return std::make_unique<DataTypes>(std::vector<DataTypePtr>{value});
}
} // end of namespace op
} // end of namespace omniruntime
