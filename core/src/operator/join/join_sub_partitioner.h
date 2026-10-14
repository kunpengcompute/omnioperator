/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 */
#ifndef OMNI_RUNTIME_JOIN_SUB_PARTITIONER_H
#define OMNI_RUNTIME_JOIN_SUB_PARTITIONER_H

#include <cstdint>
#include <vector>

#include "type/data_types.h"
#include "util/config/QueryConfig.h"
#include "vector/vector_batch.h"

namespace omniruntime::op {

struct JoinSubPartitionConfig {
    uint32_t numSubPartitions = 1;
    uint8_t startPartitionBit = 0;
    uint32_t activeSubPartition = 0;

    static JoinSubPartitionConfig FromQueryConfig(const omniruntime::config::QueryConfig &queryConfig);

    bool IsEnabled() const
    {
        return numSubPartitions > 1U;
    }
};

class JoinSubPartitioner {
public:
    explicit JoinSubPartitioner(JoinSubPartitionConfig config) : config_(config) {}

    uint32_t SubPartitionFromHash(int64_t hash) const;

    std::vector<uint32_t> PartitionRowsByKeyColumns(omniruntime::vec::VectorBatch *batch,
        const omniruntime::type::DataTypes &rowTypes, const std::vector<int32_t> &keyColIndices) const;

    const JoinSubPartitionConfig &Config() const
    {
        return config_;
    }

private:
    JoinSubPartitionConfig config_;
};

} // namespace omniruntime::op

#endif // OMNI_RUNTIME_JOIN_SUB_PARTITIONER_H
