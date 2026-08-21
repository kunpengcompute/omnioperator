/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_week function for expression system
 *              Extracts the ISO 8601 week of year (1-53) from a date (days
 *              since epoch) or a Flink TIMESTAMP (milliseconds since epoch).
 *              Mirrors Flink's WEEK(date) == EXTRACT(WEEK FROM date) semantics,
 *              where the TIMESTAMP input is TimestampData (epoch *millis*, not
 *              micros) and no session timezone is applied (wall-clock
 *              semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkWeekFunction(const std::string &name);
void RegisterFlinkWeekWithTzFunction(const std::string &name);
}
