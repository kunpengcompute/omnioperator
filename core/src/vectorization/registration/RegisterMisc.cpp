/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Registration for misc functions (spark_partition_id, uuid, etc.)
 */

#include <string>
#include "../functions/Misc.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {

void RegisterMiscFunctions(const std::string& prefix)
{
    // Register spark_partition_id: spark_partition_id() -> int32_t
    // This function returns the current Spark partition ID
    // It is a no-argument function that reads from QueryConfig
    RegisterFunction<SparkPartitionIdFunction, int32_t>(prefix + "spark_partition_id", {}, OMNI_INT);

    // Register uuid: uuid(seed: int64_t) -> varchar
    // This function generates a UUID v4 string based on seed
    // The random generator is initialized with (seed + partitionId) for partition-aware randomness
    // Each call within the same batch returns a different UUID string
    RegisterFunction<UuidFunction, std::string, int64_t>(prefix + "uuid", {OMNI_LONG}, OMNI_VARCHAR);

    // Register uuid(): uuid() -> varchar (Flink UUID(), RFC 4122 v4, 0-arg overload)
    // OmniAdaptor emits UUID() with zero arguments; this overload serves that path.
    // Non-deterministic: each row gets a new random UUID.
    RegisterFunction<UuidNoArgFunction, std::string>(prefix + "uuid", {}, OMNI_VARCHAR);
}

} // namespace omniruntime::vectorization
