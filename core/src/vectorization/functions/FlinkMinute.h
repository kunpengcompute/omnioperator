/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_minute function for expression system
 *              Extracts the minute of hour (0-59) from a Flink TIMESTAMP
 *              (milliseconds since epoch). Mirrors Flink's
 *              MINUTE(timestamp) == EXTRACT(MINUTE FROM timestamp) semantics,
 *              where the input is TimestampData (epoch *millis*, not micros)
 *              and no session timezone is applied (wall-clock semantics).
 *              Only OMNI_LONG is supported (MINUTE is a timestamp extractor,
 *              not applicable to DATE).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkMinuteFunction(const std::string &name);
void RegisterFlinkMinuteWithTzFunction(const std::string &name);
}
