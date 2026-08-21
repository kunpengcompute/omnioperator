/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Misc functions for vectorization framework (spark_partition_id, uuid, etc.)
 */

#pragma once
#include "util/compiler_util.h"
#include "vectorization/Status.h"
#include "util/config/QueryConfig.h"
#include "type/data_type.h"
#include <vector>
#include <random>
#include <cstdint>
#include <string>

namespace omniruntime::vectorization {

/// Generate a UUID v4 string in the format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
/// Following RFC 4122 for version 4 (random) UUIDs.
/// @param generator  The random number generator to use for producing random bytes.
/// @return A 36-character UUID v4 string.
std::string GenerateUuidV4(std::mt19937& generator);

/// spark_partition_id function
/// spark_partition_id() -> int32_t
/// Returns the partition ID of the current Spark partition.
/// This function reads the partition ID from QueryConfig and returns it.
/// All rows in the same partition will get the same partition ID value.
///
/// Usage: Used in Spark SQL to get the current partition ID for distributed processing.
///
/// Edge cases:
/// - If partition ID is not set in config, returns default value 0
template <typename T>
struct SparkPartitionIdFunction {
    /// Initialize the function by reading partition ID from QueryConfig
    /// This is called once per batch execution
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& config)
    {
        partitionId_ = config.sparkPartitionId();
    }

    /// call() method for no-input function
    /// Returns the partition ID stored during initialization
    ALWAYS_INLINE Status call(int32_t& result)
    {
        result = partitionId_;
        return Status::OK();
    }

private:
    int32_t partitionId_ = 0;
};

/// uuid function
/// uuid(seed: int64_t) -> varchar
/// Generates a UUID v4 string based on seed and partition ID.
///
/// This function is partition-aware: it combines the seed with partition ID to initialize
/// the random number generator, ensuring that:
/// - Same seed + same partition ID = same random sequence
/// - Different partitions with same seed = different random sequences
///
/// The output format is a standard UUID v4 string:
///   xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
/// where x is a hex digit and y is one of {8, 9, a, b}.
///
/// Usage: Used in Spark SQL's uuid() function to generate unique identifiers.
///
/// Note: This is a non-deterministic function - each call within the same batch
/// returns a different UUID value.
///
/// Edge cases:
/// - seed must be a constant value (enforced at Spark level)
/// - Different rows in the same batch get different UUID values
template <typename T>
struct UuidFunction {
    /// Initialize the function by setting up the random number generator
    /// Uses seed + partitionId to ensure partition-aware randomness
    /// This is called once per batch execution
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& config,
                    const int64_t* seed)
    {
        int32_t partitionId = config.sparkPartitionId();
        int64_t effectiveSeed = (seed != nullptr) ? (*seed + partitionId) : partitionId;
        generator_.seed(static_cast<unsigned int>(effectiveSeed));
    }

    /// callNullable() method for uuid function
    /// Generates and returns a UUID v4 string
    /// Each call generates a new random UUID value
    ALWAYS_INLINE Status callNullable(std::string& result, const int64_t* /*seed*/)
    {
        result = GenerateUuidV4(generator_);
        return Status::OK();
    }

private:
    std::mt19937 generator_;
};

/// uuid function (no-argument overload)
/// uuid() -> varchar
/// Generates an RFC 4122 version 4 (pseudo-randomly generated) UUID string, e.g.
///   xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx   (y in {8, 9, a, b})
///
/// This matches Flink SQL's UUID(), which returns java.util.UUID.randomUUID().toString().
/// It is non-deterministic: each call generates a new random UUID and the generator
/// is seeded from std::random_device (no partition awareness, aligning with Flink).
///
/// Edge cases:
/// - No inputs, result is never NULL.
/// - Different rows in the same batch get different UUID values.
template <typename T>
struct UuidNoArgFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& /*config*/)
    {
        std::random_device rd;
        generator_.seed(rd());
    }

    ALWAYS_INLINE Status call(std::string& result)
    {
        result = GenerateUuidV4(generator_);
        return Status::OK();
    }

private:
    std::mt19937 generator_;
};

} // namespace omniruntime::vectorization
