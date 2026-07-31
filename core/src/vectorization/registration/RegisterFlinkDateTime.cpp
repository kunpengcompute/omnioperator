/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Flink datetime function registration
 *
 * Registers datetime functions that mirror Flink SQL semantics (as opposed to
 * the Velox/Spark-style datetime functions registered in RegisterDateTime.cpp).
 * Each function here is named with a "flink_" prefix so it can coexist with the
 * existing function of the same extraction semantics but different unit/timezone
 * behavior (e.g. flink_year vs year).
 */

#include <string>
#include "../functions/FlinkYear.h"
#include "../functions/FlinkQuarter.h"
#include "../functions/FlinkMonth.h"

namespace omniruntime::vectorization {
void RegisterFlinkDatetimeFunctions(const std::string &prefix)
{
    // flink_year: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Distinct from
    // "year" which treats OMNI_LONG as microseconds and applies session tz.
    RegisterFlinkYearFunction(prefix + "flink_year");

    // flink_quarter: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns 1-4.
    // Distinct from "quarter" which does not support OMNI_LONG.
    RegisterFlinkQuarterFunction(prefix + "flink_quarter");

    // flink_month: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns 1-12.
    // Distinct from "month" which does not support OMNI_LONG.
    RegisterFlinkMonthFunction(prefix + "flink_month");
}
}
