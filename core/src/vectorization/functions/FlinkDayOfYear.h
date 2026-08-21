/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofyear function for expression system
 *              Extracts the day of year (1-366) from a date (days since
 *              epoch) or a Flink TIMESTAMP (milliseconds since epoch). Mirrors
 *              Flink's DAYOFYEAR(date) == EXTRACT(DOY FROM date) semantics,
 *              where the TIMESTAMP input is TimestampData (epoch *millis*, not
 *              micros) and no session timezone is applied (wall-clock
 *              semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkDayOfYearFunction(const std::string &name);
void RegisterFlinkDayOfYearWithTzFunction(const std::string &name);
}
