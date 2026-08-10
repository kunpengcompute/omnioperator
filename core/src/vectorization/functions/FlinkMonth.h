/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_month function for expression system
 *              Extracts the month of year (1-12) from a date (days since
 *              epoch) or a Flink TIMESTAMP (milliseconds since epoch). Mirrors
 *              Flink's MONTH(date) == EXTRACT(MONTH FROM date) semantics,
 *              where the TIMESTAMP input is TimestampData (epoch *millis*, not
 *              micros) and no session timezone is applied (wall-clock
 *              semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkMonthFunction(const std::string &name);
void RegisterFlinkMonthWithTzFunction(const std::string &name);
}
