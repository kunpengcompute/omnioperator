/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for dynamic UDAF registry.
 */

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "operator/aggregation/aggregator/aggregator.h"
#include "operator/aggregation/udaf/udaf_registry.h"
#include "type/data_types.h"

namespace omniruntime {
namespace op {
namespace {

class DummyUdafAggregator : public Aggregator {
public:
    DummyUdafAggregator(const type::DataTypes &inputTypes, const type::DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : Aggregator(OMNI_AGGREGATION_TYPE_UDAF, inputTypes, outputTypes, channels, inputRaw, outputPartial,
              isOverflowAsNull)
    {}

    void AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch, const int32_t filterIndex) override
    {}

    void AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch) override
    {}

    size_t GetStateSize() override
    {
        return 0;
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, const int32_t rowIndex) override
    {}

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {}

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {}
};

} // namespace

TEST(UdafRegistryTest, RegisterContainsAndCreate)
{
    const std::string name = "__test_udaf_registry_create";
    UdafRegistry::getInstance().registerUdaf(name,
        [](const type::DataTypes &inputTypes, const type::DataTypes &outputTypes, std::vector<int32_t> &channels,
            bool inputRaw, bool outputPartial, bool isOverflowAsNull) {
            return std::make_unique<DummyUdafAggregator>(
                inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
        });

    EXPECT_TRUE(UdafRegistry::getInstance().contains(name));
    EXPECT_TRUE(UdafRegistry::getInstance().getRegisteredNames().count(name) > 0);

    type::DataTypes inputTypes;
    type::DataTypes outputTypes;
    std::vector<int32_t> channels;
    auto aggregator = UdafRegistry::getInstance().create(
        name, inputTypes, outputTypes, channels, true, false, false);

    ASSERT_NE(aggregator, nullptr);
    EXPECT_EQ(aggregator->GetType(), OMNI_AGGREGATION_TYPE_UDAF);
}

TEST(UdafRegistryTest, CreateUnknownThrows)
{
    const std::string name = "__test_udaf_registry_missing";
    EXPECT_FALSE(UdafRegistry::getInstance().contains(name));

    type::DataTypes inputTypes;
    type::DataTypes outputTypes;
    std::vector<int32_t> channels;
    EXPECT_THROW(
        UdafRegistry::getInstance().create(name, inputTypes, outputTypes, channels, true, false, false),
        std::exception);
}

} // namespace op
} // namespace omniruntime
