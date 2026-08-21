/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_hour function implementation
 *
 * flink_hour(timestamp_millis) -> int32
 *
 * Mirrors Flink's HOUR(timestamp) == EXTRACT(HOUR FROM timestamp) semantics:
 *   - OMNI_LONG : Flink TIMESTAMP, represented as TimestampData = milliseconds
 *                 since epoch (NOT microseconds like the existing `hour`).
 *                 No session timezone is applied — Flink's codegen for a plain
 *                 TIMESTAMP inlines `ts.getMillisecond() % 86400000 / 3600000`
 *                 (pure modular arithmetic, no calendar decomposition, no tz).
 *
 * HOUR is a "timestamp extractor" (docs: "from SQL timestamp"), distinct from
 * the date extractors YEAR/QUARTER/MONTH/WEEK/DAYOF*. However, like them it
 * does NOT apply the session timezone for a plain TIMESTAMP: the stored millis
 * are treated as a UTC wall-clock value. (Session tz is applied only for
 * TIMESTAMP_WITH_LOCAL_TIME_ZONE, via a different codegen path.) Tests confirm
 * this: EXTRACT(HOUR FROM f18)=6 for TIMESTAMP '1996-11-10 06:55:44.333'.
 *
 * HOUR formula: Flink codegen = (millis % 86400000) / 3600000, range 0-23.
 * This is mathematically equal to std::tm.tm_hour obtained from
 * Timestamp::fromMillis(millis) -> getSeconds() -> epochToCalendarUtc (UTC):
 * the sub-millisecond part does not affect integer division by 3600000, and
 * seconds%86400/3600 == (millis%86400000)/3600000. We use tm_hour here for
 * consistency with the other flink_* extractors (same fromMillis + UTC path).
 *
 * Only OMNI_LONG is registered (HOUR is meaningless on a DATE; Flink's
 * julianExtract throws AssertionError for HOUR on a date-only Julian day).
 *
 * Returns NULL if the input is NULL, or when the value is so far out of range
 * that epochToCalendarUtc cannot decompose it.
 */

#include "FlinkHour.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
/// Resolve a session-zone-id string into a TimeZone*. The zone-id comes
/// from CommonExecCalc.getZoneId() on the Java side (e.g. "UTC",
/// "Asia/Shanghai"), so it is always a valid IANA ID - but we still fall
/// back to UTC for an unknown/empty string rather than returning nullptr,
/// matching the lenient resolution used by the rest of the vectorized
/// datetime layer.
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

/// flink_hour function
/// flink_hour(timestamp_millis) -> int32
/// OMNI_LONG = milliseconds since epoch (Flink TimestampData); hour of day
///             extracted in UTC (no session timezone — wall-clock semantics,
///             matching Flink EXTRACT(HOUR FROM <TIMESTAMP>)).
/// Returns 0-23. Returns NULL if the input is NULL (or out of range).
class FlinkHourFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        // Optional timezone arg sits on top of the stack (rightmost operand).
        // flink_X:        args = [input]
        // flink_X_with_tz: args = [input, tz]
        BaseVector *tzArg = nullptr;
        if (args.size() >= 2) {
            tzArg = args.top();
            args.pop();
        }
        const auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        const auto inputTypeId = inputArg->GetTypeId();

        // Copy NULL bits from input to result so NULL rows stay NULL.
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy(resultNulls, inputNulls, nullsSize);

        SelectivityVector rows(size);
        rows.setFromBitsNegate(inputNulls, size);

        // The tz arg (when present) is a constant session zone-id literal from
        // the Java side; resolve it once when it is a const non-null vector.
        const tz::TimeZone *constZone = nullptr;
        bool hasTz = (tzArg != nullptr);
        bool tzIsConst = hasTz && (tzArg->GetEncoding() == OMNI_ENCODING_CONST);
        if (tzIsConst && !tzArg->IsNull(0)) {
            constZone = ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, 0));
        }

        if (inputTypeId == OMNI_LONG) {
            // Flink TIMESTAMP = milliseconds since epoch (TimestampData).
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t millis = inputRaw[i];
                Timestamp ts = Timestamp::fromMillis(millis);
                // nullptr zone => UTC wall-clock; non-null zone => session-local
                // wall-clock (for TIMESTAMP_WITH_LOCAL_TIME_ZONE input).
                const tz::TimeZone *zone = constZone;
                if (hasTz && !tzIsConst) {
                    zone = tzArg->IsNull(i) ? nullptr
                        : ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, i));
                }
                std::tm tmValue = util::GetDateTime(ts, zone);
                resultRaw[i] = static_cast<int32_t>(tmValue.tm_hour);
                result->SetNotNull(i);
            });
        }
        delete inputArg;
        if (hasTz) {
            delete tzArg;
        };
    }
};
} // namespace

void RegisterFlinkHourFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_INT,
        std::make_shared<FlinkHourFunction>());
}

void RegisterFlinkHourWithTzFunction(const std::string &name)
{
    // _with_tz variant: OMNI_LONG plus an explicit VARCHAR timezone arg
    // (appended by the OmniAdaptor for TIMESTAMP_WITH_LOCAL_TIME_ZONE).
    // Reuses the same class - it detects the tz arg by args.size() >= 2.
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_INT,
        std::make_shared<FlinkHourFunction>());
}
}
