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
#include "../functions/FlinkWeek.h"
#include "../functions/FlinkDayOfYear.h"
#include "../functions/FlinkDayOfMonth.h"
#include "../functions/FlinkDayOfWeek.h"
#include "../functions/FlinkHour.h"
#include "../functions/FlinkMinute.h"
#include "../functions/FlinkSecond.h"

namespace omniruntime::vectorization {
void RegisterFlinkDatetimeFunctions(const std::string &prefix)
{
    // flink_year: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Distinct from
    // "year" which treats OMNI_LONG as microseconds and applies session tz.
    RegisterFlinkYearFunction(prefix + "flink_year");
    RegisterFlinkYearWithTzFunction(prefix + "flink_year_with_tz");

    // flink_quarter: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns 1-4.
    // Distinct from "quarter" which does not support OMNI_LONG.
    RegisterFlinkQuarterFunction(prefix + "flink_quarter");
    RegisterFlinkQuarterWithTzFunction(prefix + "flink_quarter_with_tz");

    // flink_month: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns 1-12.
    // Distinct from "month" which does not support OMNI_LONG.
    RegisterFlinkMonthFunction(prefix + "flink_month");
    RegisterFlinkMonthWithTzFunction(prefix + "flink_month_with_tz");

    // flink_week: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns ISO
    // 8601 week 1-53. Distinct from "week_of_year" which treats OMNI_LONG as
    // microseconds and also registers DATE32/TIMESTAMP.
    RegisterFlinkWeekFunction(prefix + "flink_week");
    RegisterFlinkWeekWithTzFunction(prefix + "flink_week_with_tz");

    // flink_dayofyear: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns day of
    // year 1-366. Distinct from "dayofyear" which does not support OMNI_LONG.
    RegisterFlinkDayOfYearFunction(prefix + "flink_dayofyear");
    RegisterFlinkDayOfYearWithTzFunction(prefix + "flink_dayofyear_with_tz");

    // flink_dayofmonth: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns day of
    // month 1-31. Distinct from "day"/"dayofmonth" which do not support OMNI_LONG.
    RegisterFlinkDayOfMonthFunction(prefix + "flink_dayofmonth");
    RegisterFlinkDayOfMonthWithTzFunction(prefix + "flink_dayofmonth_with_tz");

    // flink_dayofweek: OMNI_INT (date = days since epoch) / OMNI_LONG (Flink
    // TimestampData = millis since epoch, no session timezone). Returns DOW
    // (Sunday=1..Saturday=7), i.e. EXTRACT(DOW FROM date) — NOT ISODOW.
    // Distinct from "dayofweek" which does not support OMNI_LONG.
    RegisterFlinkDayOfWeekFunction(prefix + "flink_dayofweek");
    RegisterFlinkDayOfWeekWithTzFunction(prefix + "flink_dayofweek_with_tz");

    // flink_hour: OMNI_LONG (Flink TimestampData = millis since epoch, no
    // session timezone). Returns hour of day 0-23. Distinct from "hour" which
    // treats OMNI_LONG as microseconds, applies session tz, and registers
    // OMNI_TIMESTAMP. HOUR is a timestamp extractor (no OMNI_INT/DATE support).
    RegisterFlinkHourFunction(prefix + "flink_hour");
    RegisterFlinkHourWithTzFunction(prefix + "flink_hour_with_tz");

    // flink_minute: OMNI_LONG (Flink TimestampData = millis since epoch, no
    // session timezone). Returns minute of hour 0-59. Distinct from "minute"
    // which treats OMNI_LONG as microseconds and registers OMNI_TIMESTAMP.
    // MINUTE is a timestamp extractor (no OMNI_INT/DATE support).
    RegisterFlinkMinuteFunction(prefix + "flink_minute");
    RegisterFlinkMinuteWithTzFunction(prefix + "flink_minute_with_tz");

    // flink_second: OMNI_LONG (Flink TimestampData = millis since epoch, no
    // session timezone). Returns integer second of minute 0-59. Distinct from
    // "second" which treats OMNI_LONG as microseconds and registers
    // OMNI_TIMESTAMP (and a Decimal with-fraction variant). SECOND is a
    // timestamp extractor (no OMNI_INT/DATE support); this returns an INTEGER,
    // not the Decimal variant.
    RegisterFlinkSecondFunction(prefix + "flink_second");
    RegisterFlinkSecondWithTzFunction(prefix + "flink_second_with_tz");
}
}
