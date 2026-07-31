/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofmonth function for expression system
 *              Extracts the day of month (1-31) from a date (days since
 *              epoch) or a Flink TIMESTAMP (milliseconds since epoch). Mirrors
 *              Flink's DAYOFMONTH(date) == EXTRACT(DAY FROM date) semantics,
 *              where the TIMESTAMP input is TimestampData (epoch *millis*, not
 *              micros) and no session timezone is applied (wall-clock
 *              semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkDayOfMonthFunction(const std::string &name);
}
