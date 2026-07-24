/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Non-SVE stub for Spark sum(bigint) HashAgg SVE kernel
 */
#include "sum_int64_sve_hash_agg.h"

#if !(defined(__aarch64__) && defined(__linux__))

namespace omniruntime {
namespace op {

bool CanUseSveHashAggSumInt64()
{
    return false;
}

void SveHashAggSumInt64Update(AggregateState ** /*rowStates*/, size_t /*aggStateOffset*/, const int64_t * /*values*/,
    const int32_t * /*dictIds*/, const uint8_t * /*nullBits*/, int32_t /*rowCount*/,
    SumInt64SveIndicesMode /*indicesMode*/, SumInt64SveNullsMode /*nullsMode*/)
{
    // Caller must check CanUseSveHashAggSumInt64() before invoking.
}

} // namespace op
} // namespace omniruntime

#endif // !(aarch64 && linux)
