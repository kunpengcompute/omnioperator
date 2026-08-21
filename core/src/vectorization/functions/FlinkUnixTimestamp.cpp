/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_unix_timestamp function implementation
 *
 * flink_unix_timestamp() -> int64 (OMNI_LONG, current epoch seconds)
 * flink_unix_timestamp(string1[, string2][, tz]) -> int64 (epoch seconds)
 *
 * Mirrors Flink's UNIX_TIMESTAMP semantics:
 *   - No-arg: returns System.currentTimeMillis() / 1000 (current epoch seconds),
 *     non-deterministic (recalculated per record).
 *   - 1-arg string: parses with default format 'yyyy-MM-dd HH:mm:ss', applies the
 *     session timezone, returns epoch seconds.
 *   - 2-arg string: parses with the given format, applies the session timezone,
 *     returns epoch seconds.
 *
 * Timezone (Plan A — explicit VARCHAR tz arg, same pattern as flink_*_with_tz):
 *   The session timezone is passed as an explicit VARCHAR literal argument appended
 *   by the OmniAdaptor (DateTimeExprHandlers.handleUnixTimestamp), sourced from
 *   CommonExecCalc.getZoneId() (i.e. Flink's table.local-time-zone). This is the
 *   Flink-native way to convey the session tz to the vectorized layer — the
 *   QueryConfig.session_timezone key is a Spark/Gluten mechanism and is NOT
 *   populated on the Flink path. The tz arg sits at the top of the stack
 *   (rightmost operand) and is resolved via ResolveSessionTimeZone.
 *
 * Parse failure: returns Long.MIN_VALUE (-9223372036854775808), NOT NULL and NOT
 * an exception — matching Flink's internalParseTimestampMillis which returns
 * Long.MIN_VALUE on ParseException.
 *
 * NULL input: returns NULL (Flink codegen null-checks inputs before the method
 * call, so a NULL string yields SQL NULL, distinct from the Long.MIN_VALUE
 * parse-failure sentinel).
 */

#include "FlinkUnixTimestamp.h"
#include "SparkDateTimeFormat.h"
#include "DateTimeZoneConversion.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <chrono>
#include <limits>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

static constexpr const char *kDefaultFormat = "yyyy-MM-dd HH:mm:ss";
// Flink parse-failure sentinel: Long.MIN_VALUE.
constexpr int64_t kLongMinValue = std::numeric_limits<int64_t>::min();

/// Resolve a session-zone-id string into a TimeZone*. The zone-id comes from
/// CommonExecCalc.getZoneId() on the Java side (e.g. "UTC", "Asia/Shanghai"),
/// so it is always a valid IANA ID - but we still fall back to UTC for an
/// unknown/empty string rather than returning nullptr, matching the lenient
/// resolution used by the rest of the vectorized datetime layer (same as
/// FlinkHour.cpp::ResolveSessionTimeZone).
static const tz::TimeZone *ResolveSessionTimeZone(const std::string_view &tzView)
{
    if (tzView.empty()) {
        return tz::locateZone("UTC", /*failOnError=*/false);
    }
    if (const tz::TimeZone *zone = tz::locateZone(tzView, /*failOnError=*/false)) {
        return zone;
    }
    return tz::locateZone("UTC", /*failOnError=*/false);
}

// Convert local (session-tz) micros to UTC micros, applying the session timezone.
// Mirrors ToTimestamp.cpp::ConvertLocalMicrosToUtc.
int64_t ConvertLocalMicrosToUtc(int64_t localMicros, const tz::TimeZone *timeZone,
    datetime::LocalToUtcState &state)
{
    if (timeZone == nullptr) {
        return localMicros;
    }
    const Timestamp timestamp = Timestamp::fromMicros(localMicros);
    const auto utcSeconds = datetime::ConvertLocalToUtc(
        std::chrono::seconds(timestamp.getSeconds()), timeZone, &state);
    return utcSeconds.count() * Timestamp::kMicrosecondsInSecond +
        (localMicros % Timestamp::kMicrosecondsInSecond);
}

/// flink_unix_timestamp() / (string[, format]) -> int64 (OMNI_LONG, epoch seconds)
/// 0-arg: current epoch seconds (non-deterministic). No timezone.
/// 1-arg: default format, UTC (no tz arg). failure -> Long.MIN_VALUE.
/// 2-arg: explicit format, UTC (no tz arg). failure -> Long.MIN_VALUE.
/// NULL input -> NULL (distinct from Long.MIN_VALUE parse-failure sentinel).
///
/// This is the no-tz variant. The session-timezone-aware variant is
/// FlinkUnixTimestampWithTzFunction (registered as flink_unix_timestamp_with_tz),
/// which takes a trailing VARCHAR tz arg (Plan A).
class FlinkUnixTimestampFunction : public VectorFunction {
public:
    // Only the 0-arg form is registered under flink_unix_timestamp (see
    // RegisterFlinkUnixTimestampFunction). String forms are handled by the
    // _with_tz subclass (FlinkUnixTimestampWithTzFunction) which overrides
    // Apply to pop the trailing tz arg before delegating to ApplyString.
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        ApplyNoArg(args, outputType, result, context);
    }

private:
    void ApplyNoArg(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const
    {
        const auto rowSize = context->GetResultRowSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, rowSize);
        }
        auto *resultVec = static_cast<Vector<int64_t> *>(result);

        // Current epoch seconds (System.currentTimeMillis() / 1000 equivalent).
        auto now = std::chrono::system_clock::now();
        int64_t seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        for (int32_t row = 0; row < rowSize; ++row) {
            resultVec->SetValue(row, seconds);
            result->SetNotNull(row);
        }
    }

protected:
    // Shared string-parsing core used by both the no-tz and _with_tz variants.
    // tzArg == nullptr => UTC (ConvertLocalMicrosToUtc is a no-op).
    void ApplyString(BaseVector *inputArg, BaseVector *formatArg, BaseVector *tzArg,
        BaseVector *&result) const
    {
        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        }
        auto *resultVec = static_cast<Vector<int64_t> *>(result);

        const bool hasFormatArg = (formatArg != nullptr);
        const bool formatIsConst = hasFormatArg && (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        constexpr bool isLegacy = true;

        // Pre-compile const/default format once.
        datetime::CompiledParseFormat constCompiledFormat;
        if (hasFormatArg) {
            if (formatIsConst && !formatArg->IsNull(0)) {
                std::string_view formatView = VectorHelper::GetStringValueFromVector(formatArg, 0);
                constCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
            }
        } else {
            constCompiledFormat = datetime::CompileParseFormat(kDefaultFormat, isLegacy);
        }

        // Resolve the session timezone from the trailing tz arg (Plan A).
        // nullptr tz => UTC (ConvertLocalMicrosToUtc is a no-op for nullptr).
        const tz::TimeZone *sessionTz = nullptr;
        const bool hasTz = (tzArg != nullptr);
        const bool tzIsConst = hasTz && (tzArg->GetEncoding() == OMNI_ENCODING_CONST);
        if (hasTz && tzIsConst && !tzArg->IsNull(0)) {
            sessionTz = ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, 0));
        }
        datetime::LocalToUtcState timeZoneState;

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            if (hasFormatArg && !formatIsConst && formatArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view inputStr = VectorHelper::GetStringValueFromVector(inputArg, row);

            datetime::CompiledParseFormat rowCompiledFormat;
            const datetime::CompiledParseFormat *compiledFormat = &constCompiledFormat;
            if (hasFormatArg && !formatIsConst) {
                std::string_view formatView = VectorHelper::GetStringValueFromVector(formatArg, row);
                rowCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
                compiledFormat = &rowCompiledFormat;
            }

            // Resolve per-row tz for non-const tz arg.
            const tz::TimeZone *rowTz = sessionTz;
            if (hasTz && !tzIsConst) {
                rowTz = tzArg->IsNull(row) ? nullptr
                    : ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, row));
            }

            int64_t resultMicros = 0;
            if (datetime::ParseDateTimeString(inputStr, *compiledFormat, resultMicros)) {
                // Apply session timezone: local micros -> UTC micros.
                resultMicros = ConvertLocalMicrosToUtc(resultMicros, rowTz, timeZoneState);
                int64_t seconds = resultMicros / Timestamp::kMicrosecondsInSecond;
                resultVec->SetValue(row, seconds);
                result->SetNotNull(row);
            } else {
                // Flink semantics: parse failure returns Long.MIN_VALUE (not NULL).
                resultVec->SetValue(row, kLongMinValue);
                result->SetNotNull(row);
            }
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
        if (formatArg != nullptr) {
            delete formatArg;
        }
        if (tzArg != nullptr) {
            delete tzArg;
        }
    }
};

/// flink_unix_timestamp_with_tz(string[, format], tz) -> int64 (OMNI_LONG, epoch seconds)
/// Same as FlinkUnixTimestampFunction but the trailing VARCHAR tz arg is always
/// present (appended by the OmniAdaptor from CommonExecCalc.getZoneId()).
///   1-arg + tz: [input, tz]        -> default format, session tz
///   2-arg + tz: [input, format, tz] -> explicit format, session tz
class FlinkUnixTimestampWithTzFunction : public FlinkUnixTimestampFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        const auto argCount = args.size();
        // The tz arg is always the topmost (rightmost) operand.
        BaseVector *tzArg = args.top();
        args.pop();
        BaseVector *formatArg = nullptr;
        if (argCount >= 3) {
            // [input, format, tz]
            formatArg = args.top();
            args.pop();
        }
        // [input, tz] (1-arg + tz) or [input, format, tz] (2-arg + tz, format already popped)
        auto inputArg = args.top();
        args.pop();

        ApplyString(inputArg, formatArg, tzArg, result);
    }
};

} // namespace

void RegisterFlinkUnixTimestampFunction(const std::string &name)
{
    auto func = std::make_shared<FlinkUnixTimestampFunction>();
    VectorFunction::RegisterVectorFunction(name, {}, OMNI_LONG, func);
}

void RegisterFlinkUnixTimestampWithTzFunction(const std::string &name)
{
    auto func = std::make_shared<FlinkUnixTimestampWithTzFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_CHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_CHAR, OMNI_VARCHAR}, OMNI_LONG, func);
}

} // namespace omniruntime::vectorization