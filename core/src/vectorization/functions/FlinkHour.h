/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_hour function for expression system
 *              Extracts the hour of day (0-23) from a Flink TIMESTAMP
 *              (milliseconds since epoch). Mirrors Flink's
 *              HOUR(timestamp) == EXTRACT(HOUR FROM timestamp) semantics,
 *              where the input is TimestampData (epoch *millis*, not micros)
 *              and no session timezone is applied (wall-clock semantics).
 *              Only OMNI_LONG is supported (HOUR is a timestamp extractor,
 *              not applicable to DATE).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkHourFunction(const std::string &name);
}
