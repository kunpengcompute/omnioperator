/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: DateTime function registration
 */

#include <string>
#include "../functions/Hour.h"
#include "../functions/Minute.h"
#include "../functions/Second.h"
#include "../functions/Month.h"
#include "../functions/Quarter.h"
#include "../functions/Year.h"
#include "../functions/Day.h"
#include "../functions/DayOfWeek.h"
#include "../functions/DayOfYear.h"
#include "../functions/WeekOfYear.h"
#include "../functions/Weekday.h"
#include "../functions/YearOfWeek.h"
#include "../functions/Trunc.h"
#include "../functions/AddMonths.h"
#include "../functions/DateAdd.h"
#include "../functions/DateSub.h"
#include "../functions/DateArithmetic.h"
#include "../functions/TimestampConversion.h"
#include "../functions/MakeDate.h"
#include "../functions/MakeTimestamp.h"
#include "../functions/UnixSeconds.h"
#include "../functions/UnixMillis.h"
#include "../functions/UnixMicros.h"
#include "../functions/UnixDate.h"
#include "../functions/DateFromUnixDate.h"
#include "../functions/ToTimestamp.h"
#include "../functions/ToDate.h"
#include "../functions/ToUtcTimestamp.h"
#include "../functions/LastDay.h"
#include "../functions/MonthsBetween.h"
#include "../functions/NextDay.h"
#include "../functions/DateDiff.h"
#include "../functions/DateFormat.h"
#include "../functions/FromUnixTime.h"
#include "../functions/CurrentDateTimeFunctions.h"
#include "../functions/DateTrunc.h"
#include "../functions/Floor.h"
#include "../functions/Ceil.h"
#include "../functions/Time.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterDatetimeFunctions(const std::string &prefix)
{
    RegisterHourFunction(prefix + "hour");
    RegisterMinuteFunction(prefix + "minute");
    RegisterSecondFunction(prefix + "second");
    RegisterMonthFunction(prefix + "month");
    RegisterQuarterFunction(prefix + "quarter");
    RegisterYearFunction(prefix + "year");
    RegisterDayFunction(prefix + "day");
    RegisterDayFunction(prefix + "dayofmonth");
    RegisterDayOfWeekFunction(prefix + "dayofweek");
    RegisterDayOfYearFunction(prefix + "dayofyear");
    RegisterWeekOfYearFunction(prefix + "week_of_year");
    RegisterWeekdayFunction(prefix + "weekday");
    RegisterYearOfWeekFunction(prefix + "year_of_week");
    // Register as "trunc_date" to match Gluten mapping (Substrait "trunc" -> "trunc_date")
    // and codegen layer registration
    RegisterTruncFunction(prefix + "trunc_date");

    RegisterDateTruncFunction(prefix + "date_trunc");
    RegisterAddMonthsFunction(prefix + "add_months");
    RegisterDateAddFunction(prefix + "date_add");
    RegisterDateSubFunction(prefix + "date_sub");
    RegisterTimestampMicrosFunction(prefix + "timestamp_micros");
    RegisterTimestampMillisFunction(prefix + "timestamp_millis");
    RegisterTimestampSecondsFunction(prefix + "timestamp_seconds");

    RegisterMakeDateFunction(prefix + "make_date");
    RegisterMakeTimestampFunction(prefix + "make_timestamp");
    RegisterUnixSecondsFunction(prefix + "unix_seconds");
    RegisterUnixMillisFunction(prefix + "unix_millis");
    RegisterUnixMicrosFunction(prefix + "unix_micros");
    RegisterUnixDateFunction(prefix + "unix_date");
    RegisterDateFromUnixDateFunction(prefix + "date_from_unix_date");

    RegisterToTimestampFunction(prefix + "get_timestamp");
    RegisterToDateFunction(prefix + "to_date");
    RegisterToUnixTimestampFunction(prefix + "to_unix_timestamp");
    RegisterToUnixTimestampFunction(prefix + "unix_timestamp");

    RegisterToUtcTimestampFunction(prefix + "to_utc_timestamp");
    RegisterFromUtcTimestampFunction(prefix + "from_utc_timestamp");

    RegisterLastDayFunction(prefix + "last_day");

    RegisterMonthsBetweenFunction(prefix + "months_between");

    RegisterNextDayFunction(prefix + "next_day");

    RegisterDateDiffFunction(prefix + "date_diff");

    // Registered as "DateFormat" (CamelCase) to match Gluten SubstraitParser mapping:
    // {"date_format", {FUNCTION_OMNI_EXPR_TYPE, "DateFormat"}}
    RegisterDateFormatFunction(prefix + "DateFormat");

    RegisterFromUnixTimeFunction(prefix + "from_unixtime");

    RegisterFunction<LocalTimeFunction, int64_t>(prefix + "flink_localtime", {}, OMNI_LONG);

    RegisterFunction<LocalTimestampFunction, int64_t>(prefix + "flink_localtimestamp", {}, OMNI_LONG);

    RegisterFunction<CurrentTimestampFunction, int64_t>(prefix + "current_timestamp", {}, OMNI_LONG); //因为codgen有同名函数，所以不能加前缀

    RegisterFunction<CurrentRowTimestampFunction, int64_t>(prefix + "flink_current_row_timestamp", {}, OMNI_LONG);

    RegisterFunction<CurrentDateFunction, int32_t>(prefix + "flink_current_date", {}, OMNI_INT);

    RegisterFloorFunction(prefix + "flink_floor_time");

    RegisterCeilFunction(prefix + "flink_ceil_time");

    RegisterTimeFunction(prefix + "time");
}
}
