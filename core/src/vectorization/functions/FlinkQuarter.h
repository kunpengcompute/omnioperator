/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_quarter function for expression system
 *              Extracts the quarter of year (1-4) from a date (days since
 *              epoch) or a Flink TIMESTAMP (milliseconds since epoch). Mirrors
 *              Flink's QUARTER(date) == EXTRACT(QUARTER FROM date) semantics,
 *              where the TIMESTAMP input is TimestampData (epoch *millis*, not
 *              micros) and no session timezone is applied (wall-clock
 *              semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkQuarterFunction(const std::string &name);
}
