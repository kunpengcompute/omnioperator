/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 */
#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"
#include "operator/hash_util.h"
#include "operator/join/join_sub_partitioner.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/omni_exception.h"
#include "vector/large_string_container.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

JoinSubPartitionConfig TestConfig(uint8_t bits = 2, uint8_t startBit = 0)
{
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 1U << bits;
    cfg.startPartitionBit = startBit;
    cfg.activeSubPartition = 0;
    return cfg;
}

VectorBatch *MakeLongBatch(const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(values.size());
    auto *vec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i) {
        vec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(vec);
    return batch;
}

VectorBatch *MakeStringBatch(const std::vector<std::string> &values)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    auto *batch = new VectorBatch(values.size());
    auto *vec = new StringVector(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i) {
        vec->SetValue(static_cast<int32_t>(i), std::string_view(values[i]));
    }
    batch->Append(vec);
    return batch;
}

} // namespace

TEST(JoinSubPartitionerTest, BuildsConfigFromQueryConfig)
{
    std::unordered_map<std::string, std::string> queryValues;
    queryValues.emplace(omniruntime::config::QueryConfig::kSpillNumPartitionBits, "2");
    queryValues.emplace(omniruntime::config::QueryConfig::kSpillStartPartitionBit, "1");
    omniruntime::config::QueryConfig queryConfig(std::move(queryValues));
    auto cfg = JoinSubPartitionConfig::FromQueryConfig(queryConfig);
    EXPECT_EQ(cfg.numSubPartitions, 4U);
    EXPECT_EQ(cfg.startPartitionBit, 1U);
    EXPECT_EQ(cfg.activeSubPartition, 0U);
    EXPECT_TRUE(cfg.IsEnabled());
}

TEST(JoinSubPartitionerTest, SparkBitsMapsToPowerOfTwoSubPartitions)
{
    auto fromBits = [](const char *bits) {
        std::unordered_map<std::string, std::string> queryValues;
        queryValues.emplace(omniruntime::config::QueryConfig::kSpillNumPartitionBits, bits);
        queryValues.emplace(omniruntime::config::QueryConfig::kSpillStartPartitionBit, "0");
        return JoinSubPartitionConfig::FromQueryConfig(omniruntime::config::QueryConfig(std::move(queryValues)));
    };
    EXPECT_EQ(fromBits("2").numSubPartitions, 4U);
    EXPECT_EQ(fromBits("4").numSubPartitions, 16U);
    EXPECT_EQ(fromBits("6").numSubPartitions, 64U);
    EXPECT_EQ(fromBits("0").numSubPartitions, 1U);
}

TEST(JoinSubPartitionerTest, UnsetBitsDefaultToSixteenSubPartitions)
{
    omniruntime::config::QueryConfig queryConfig(std::unordered_map<std::string, std::string>{});
    auto cfg = JoinSubPartitionConfig::FromQueryConfig(queryConfig);
    EXPECT_EQ(cfg.numSubPartitions, 16U);
}

TEST(JoinSubPartitionerTest, PartitionsLongRows)
{
    auto *batch = MakeLongBatch({1, 2, 3, 4});
    DataTypes types(std::vector<DataTypePtr>({ LongDataType::Instance() }));
    JoinSubPartitioner partitioner(TestConfig());
    auto partitions = partitioner.PartitionRowsByKeyColumns(batch, types, {0});

    ASSERT_EQ(partitions.size(), 4U);
    for (size_t i = 0; i < partitions.size(); ++i) {
        const int64_t value = static_cast<int64_t>(i + 1);
        EXPECT_EQ(partitions[i], partitioner.SubPartitionFromHash(HashUtil::HashValue(value)));
    }
    delete batch;
}

TEST(JoinSubPartitionerTest, PartitionsVarcharRows)
{
    std::vector<std::string> values = {"a", "bb", "ccc"};
    auto *batch = MakeStringBatch(values);
    DataTypes types(std::vector<DataTypePtr>({ VarcharDataType::Instance() }));
    JoinSubPartitioner partitioner(TestConfig());
    auto partitions = partitioner.PartitionRowsByKeyColumns(batch, types, {0});

    ASSERT_EQ(partitions.size(), values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        auto *data = reinterpret_cast<int8_t *>(const_cast<char *>(values[i].data()));
        EXPECT_EQ(partitions[i], partitioner.SubPartitionFromHash(
            HashUtil::HashValue(data, static_cast<int32_t>(values[i].size()))));
    }
    delete batch;
}

TEST(JoinSubPartitionerTest, NullKeyUsesActiveSubPartition)
{
    auto *batch = MakeLongBatch({1, 2, 3});
    batch->Get(0)->SetNull(1);
    DataTypes types(std::vector<DataTypePtr>({ LongDataType::Instance() }));
    JoinSubPartitionConfig cfg = TestConfig();
    cfg.activeSubPartition = 3;
    JoinSubPartitioner partitioner(cfg);
    auto partitions = partitioner.PartitionRowsByKeyColumns(batch, types, {0});

    ASSERT_EQ(partitions.size(), 3U);
    EXPECT_EQ(partitions[1], cfg.activeSubPartition);
    delete batch;
}

TEST(JoinSubPartitionerTest, PartitionsMultiLongKeys)
{
    auto *batch = new VectorBatch(3);
    auto *left = new Vector<int64_t>(3);
    auto *right = new Vector<int64_t>(3);
    for (int32_t i = 0; i < 3; ++i) {
        left->SetValue(i, i + 1);
        right->SetValue(i, 10 + i);
    }
    batch->Append(left);
    batch->Append(right);
    DataTypes types(std::vector<DataTypePtr>({ LongDataType::Instance(), LongDataType::Instance() }));
    JoinSubPartitioner partitioner(TestConfig());
    auto partitions = partitioner.PartitionRowsByKeyColumns(batch, types, {0, 1});

    ASSERT_EQ(partitions.size(), 3U);
    for (int32_t i = 0; i < 3; ++i) {
        const auto h1 = HashUtil::HashValue(static_cast<int64_t>(i + 1));
        const auto h2 = HashUtil::HashValue(static_cast<int64_t>(10 + i));
        EXPECT_EQ(partitions[i], partitioner.SubPartitionFromHash(HashUtil::CombineHash(h1, h2)));
    }
    delete batch;
}

TEST(JoinSubPartitionerTest, RejectsUnsupportedKeyType)
{
    auto *batch = new VectorBatch(1);
    auto *vec = new Vector<double>(1);
    vec->SetValue(0, 1.0);
    batch->Append(vec);
    DataTypes types(std::vector<DataTypePtr>({ DoubleDataType::Instance() }));
    JoinSubPartitioner partitioner(TestConfig());

    EXPECT_THROW(partitioner.PartitionRowsByKeyColumns(batch, types, {0}), omniruntime::exception::OmniException);
    delete batch;
}
