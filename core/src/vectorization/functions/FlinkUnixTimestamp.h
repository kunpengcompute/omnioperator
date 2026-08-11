/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_unix_timestamp function for expression system
 *              - UNIX_TIMESTAMP() -> current epoch seconds (non-deterministic)
 *              - UNIX_TIMESTAMP(string1) -> parse with default format, seconds
 *              - UNIX_TIMESTAMP(string1, string2) -> parse with format, seconds
 *              Mirrors Flink's UNIX_TIMESTAMP semantics: returns BIGINT seconds,
 *              and returns Long.MIN_VALUE (not NULL) on parse failure.
 *              Plan A: the session timezone is conveyed as an explicit trailing
 *              VARCHAR arg via the _with_tz variant (flink_unix_timestamp_with_tz),
 *              sourced from Flink's table.local-time-zone (CommonExecCalc.getZoneId()).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkUnixTimestampFunction(const std::string &name);
void RegisterFlinkUnixTimestampWithTzFunction(const std::string &name);
}
