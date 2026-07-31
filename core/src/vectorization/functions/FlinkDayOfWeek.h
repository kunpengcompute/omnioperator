/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofweek function for expression system
 *              Extracts the day of week (1-7, Sunday=1) from a date (days
 *              since epoch) or a Flink TIMESTAMP (milliseconds since epoch).
 *              Mirrors Flink's DAYOFWEEK(date) == EXTRACT(DOW FROM date)
 *              semantics (DOW, NOT ISODOW), where the TIMESTAMP input is
 *              TimestampData (epoch *millis*, not micros) and no session
 *              timezone is applied (wall-clock semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkDayOfWeekFunction(const std::string &name);
}
