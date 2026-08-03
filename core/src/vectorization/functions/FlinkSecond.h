/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_second function for expression system
 *              Extracts the second of minute (0-59, integer) from a Flink
 *              TIMESTAMP (milliseconds since epoch). Mirrors Flink's
 *              SECOND(timestamp) == EXTRACT(SECOND FROM timestamp) semantics,
 *              where the input is TimestampData (epoch *millis*, not micros)
 *              and no session timezone is applied (wall-clock semantics).
 *              Only OMNI_LONG is supported (SECOND is a timestamp extractor,
 *              not applicable to DATE). Returns an INTEGER second (0-59), not
 *              the Decimal with-fraction variant.
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkSecondFunction(const std::string &name);
}
