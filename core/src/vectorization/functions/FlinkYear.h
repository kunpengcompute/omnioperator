/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_year function for expression system
 *              Extracts the year from a date (days since epoch) or a Flink
 *              TIMESTAMP (milliseconds since epoch). Mirrors Flink's
 *              YEAR(date) == EXTRACT(YEAR FROM date) semantics, where the
 *              TIMESTAMP input is TimestampData (epoch *millis*, not micros)
 *              and no session timezone is applied (wall-clock semantics).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkYearFunction(const std::string &name);
void RegisterFlinkYearWithTzFunction(const std::string &name);
}
