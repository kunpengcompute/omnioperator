/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CONVERT_TZ function implementation for vectorized execution
 *
 * convert_tz(datetimeString, tzFrom, tzTo) -> string
 *
 * Mirrors Flink's DateTimeUtils.convertTz(dateStr, tzFrom, tzTo):
 *   1. Parse `dateStr` with the default format 'yyyy-MM-dd HH:mm:ss' as a
 *      local wall-clock time in `tzFrom`.
 *   2. Convert it to a UTC instant (epoch seconds).
 *   3. Render that instant as a local wall-clock time in `tzTo` using the
 *      same 'yyyy-MM-dd HH:mm:ss' format.
 *
 * Returns NULL when any input is NULL or when the datetime string cannot be
 * parsed. Time-zone resolution mirrors java.util.TimeZone.getTimeZone: unknown
 * IDs (including bare offsets like "+08:00" that lack a "GMT" prefix) silently
 * fall back to GMT (UTC+0), exactly as Flink does.
 */

#include "ConvertTz.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

using seconds = std::chrono::seconds;
using days = omniruntime::date::days;

std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row)
{
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        return std::string_view();
    }
}

const tz::TimeZone *ResolveTimeZone(const std::string_view &tzView)
{
    if (tzView.empty()) {
        return nullptr;
    }
    // Bare offset (no "GMT"/"UTC"/"UT" prefix): JDK treats it as an unknown ID
    // and falls back to GMT. Skip the locateZone lookup that would otherwise
    // honour it as a true fixed-offset zone.
    const char c0 = tzView.front();
    if ((c0 == '+' || c0 == '-')) {
        // tz::locateZone resolves "+00:00"/"-00:00" to UTC already, but for
        // consistency with Flink (which also falls back to GMT) and any other
        // bare offset, fall back to GMT.
        return tz::locateZone("UTC", /*failOnError=*/false);
    }
    if (const tz::TimeZone *zone = tz::locateZone(tzView, /*failOnError=*/false)) {
        return zone;
    }
    // Unknown ID: Flink's TimeZone.getTimeZone never returns null — it silently
    // falls back to GMT. Mirror that to stay result-compatible.
    return tz::locateZone("UTC", /*failOnError=*/false);
}

/// Strictly parse 'yyyy-MM-dd HH:mm:ss' (19 chars). Returns false on any mismatch.
/// Mirrors Flink's SimpleDateFormat("yyyy-MM-dd HH:mm:ss") for the canonical form.
bool ParseTimestampString(const std::string_view &str, int &year, int &month, int &day,
    int &hour, int &minute, int &second)
{
    // Strict canonical form: 19 chars, 'yyyy-MM-dd HH:mm:ss'.
    if (str.size() != 19) {
        return false;
    }
    const char *s = str.data();
    // Required separator layout: positions 4,7='-', 10=' ', 13,16=':'.
    if (s[4] != '-' || s[7] != '-' || s[10] != ' ' || s[13] != ':' || s[16] != ':') {
        return false;
    }
    for (int i = 0; i < 19; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) {
            continue;
        }
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
    }

    auto twoDigits = [&](int pos) { return (s[pos] - '0') * 10 + (s[pos + 1] - '0'); };
    auto fourDigits = [&](int pos) {
        return (s[pos] - '0') * 1000 + (s[pos + 1] - '0') * 100 + (s[pos + 2] - '0') * 10
            + (s[pos + 3] - '0');
    };

    year = fourDigits(0);
    month = twoDigits(5);
    day = twoDigits(8);
    hour = twoDigits(11);
    minute = twoDigits(14);
    second = twoDigits(17);

    // Basic range validation (matches Flink's SimpleDateFormat behaviour:
    // out-of-range fields would fail to parse).
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }
    if (hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    return true;
}

/// Build a local-time seconds value (seconds since epoch *as if the wall-clock
/// fields were UTC*) from broken-down y/m/d h/m/s. This is the convention used
/// by the date library's local_time: the duration since epoch of the given
/// civil fields, ignoring any zone offset.
seconds BuildLocalSeconds(int yr, int mon, int dy, int hr, int min, int sec)
{
    namespace date = omniruntime::date;
    auto ymd = date::year{yr} / date::month{static_cast<unsigned>(mon)} / date::day{static_cast<unsigned>(dy)};
    date::local_days localDays{ymd};
    auto localSecs = std::chrono::duration_cast<seconds>(localDays.time_since_epoch());
    localSecs += std::chrono::hours{hr};
    localSecs += std::chrono::minutes{min};
    localSecs += std::chrono::seconds{sec};
    return localSecs;
}

/// Format a local-time seconds value (civil fields) as 'yyyy-MM-dd HH:mm:ss'.
/// `localSecs` is the duration since epoch *as if the wall-clock were UTC*.
std::string FormatLocalSeconds(seconds localSecs)
{
    namespace date = omniruntime::date;
    // Build a sys_time from the same duration to decompose into civil fields;
    // since both local_time and sys_time share the epoch convention, the civil
    // fields derived here equal the wall-clock fields we want to render.
    date::sys_seconds sysPt{localSecs};
    auto dp = date::floor<days>(sysPt);
    date::year_month_day ymd{dp};
    date::hh_mm_ss<seconds> tod{sysPt - dp};

    int yr = static_cast<int>(static_cast<int64_t>(ymd.year()));
    unsigned mon = static_cast<unsigned>(ymd.month());
    unsigned dy = static_cast<unsigned>(ymd.day());
    int hr = static_cast<int>(tod.hours().count());
    int min = static_cast<int>(tod.minutes().count());
    int sec = static_cast<int>(tod.seconds().count());

    char buf[20];
    // Zero-padded 'yyyy-MM-dd HH:mm:ss'. Year is rendered with %04d to keep the
    // sign and 4-digit width consistent with Flink's SimpleDateFormat for years
    // in the supported range.
    std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02d:%02d:%02d",
        yr, mon, dy, hr, min, sec);
    return std::string(buf);
}

/// ConvertTz(dateStr, tzFrom, tzTo) -> string.
class ConvertTzVectorFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 3) {
            OMNI_THROW("ConvertTz function Error", "Expected 3 arguments (datetime, tzFrom, tzTo)");
        }

        // Stack top is the last argument: pop right-to-left -> (datetime, tzFrom, tzTo).
        auto tzToArg = args.top();
        args.pop();
        auto tzFromArg = args.top();
        args.pop();
        auto datetimeArg = args.top();
        args.pop();

        const auto size = datetimeArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
        }

        // Const-vs-flat resolution for the two timezone args.
        bool tzFromIsConst = (tzFromArg->GetEncoding() == OMNI_ENCODING_CONST);
        bool tzToIsConst = (tzToArg->GetEncoding() == OMNI_ENCODING_CONST);
        const tz::TimeZone *constZoneFrom = nullptr;
        const tz::TimeZone *constZoneTo = nullptr;
        bool constZoneFromValid = false; // false means the const tz input was NULL
        bool constZoneToValid = false;

        if (tzFromIsConst) {
            if (!tzFromArg->IsNull(0)) {
                constZoneFrom = ResolveTimeZone(GetStringValueFromVector(tzFromArg, 0));
                constZoneFromValid = (constZoneFrom != nullptr);
            }
        }
        if (tzToIsConst) {
            if (!tzToArg->IsNull(0)) {
                constZoneTo = ResolveTimeZone(GetStringValueFromVector(tzToArg, 0));
                constZoneToValid = (constZoneTo != nullptr);
            }
        }

        auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);

        for (int32_t row = 0; row < size; ++row) {
            // NULL datetime -> NULL.
            if (datetimeArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            // Non-const timezone NULL rows -> NULL.
            if (!tzFromIsConst && tzFromArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            if (!tzToIsConst && tzToArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            // Resolve per-row timezones.
            const tz::TimeZone *zoneFrom = constZoneFrom;
            bool zoneFromValid = constZoneFromValid;
            if (!tzFromIsConst) {
                zoneFrom = ResolveTimeZone(GetStringValueFromVector(tzFromArg, row));
                zoneFromValid = (zoneFrom != nullptr);
            }
            const tz::TimeZone *zoneTo = constZoneTo;
            bool zoneToValid = constZoneToValid;
            if (!tzToIsConst) {
                zoneTo = ResolveTimeZone(GetStringValueFromVector(tzToArg, row));
                zoneToValid = (zoneTo != nullptr);
            }
            if (!zoneFromValid || !zoneToValid) {
                result->SetNull(row);
                continue;
            }

            // Parse the datetime string.
            std::string_view dtView = GetStringValueFromVector(datetimeArg, row);
            int year, month, day, hour, minute, second;
            if (!ParseTimestampString(dtView, year, month, day, hour, minute, second)) {
                result->SetNull(row);
                continue;
            }

            // Convert: local(in tzFrom) -> UTC instant -> local(in tzTo) -> string.
            try {
                seconds localFrom = BuildLocalSeconds(year, month, day, hour, minute, second);
                // Correct nonexistent DST gap times before to_sys so they don't throw.
                // (ambiguous fall-back times are resolved by kEarliest.)
                seconds corrected = zoneFrom->correct_nonexistent_time(localFrom);
                seconds sysSecs = zoneFrom->to_sys(corrected, tz::TimeZone::TChoose::kEarliest);
                seconds localTo = zoneTo->to_local(sysSecs);
                std::string formatted = FormatLocalSeconds(localTo);
                resultVec->SetValue(row, std::string_view(formatted));
                result->SetNotNull(row);
            } catch (const std::exception &e) {
                // Any unresolved DST/validation issue -> NULL (matches Flink's
                // ParseException -> null policy for unrepresentable instants).
                result->SetNull(row);
            } catch (...) {
                result->SetNull(row);
            }
        }

        if (datetimeArg != nullptr) {
            delete datetimeArg;
        }
        if (tzFromArg != nullptr) {
            delete tzFromArg;
        }
        if (tzToArg != nullptr) {
            delete tzToArg;
        }
    }
};

} // namespace

void RegisterConvertTzFunction(const std::string &name)
{
    auto func = std::make_shared<ConvertTzVectorFunction>();
    // 3 string arguments: (datetime, tzFrom, tzTo). Cover all VARCHAR/CHAR
    // combinations, mirroring RegisterToUtcTimestampFunction's pattern.
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_CHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_CHAR, OMNI_VARCHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_CHAR, OMNI_CHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR, OMNI_CHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_CHAR, OMNI_VARCHAR}, OMNI_VARCHAR, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_CHAR, OMNI_CHAR}, OMNI_VARCHAR, func);
}

} // namespace omniruntime::vectorization
