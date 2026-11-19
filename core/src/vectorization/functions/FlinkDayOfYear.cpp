/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofyear function implementation
 *
 * flink_dayofyear(date) -> int32, flink_dayofyear(timestamp) -> int32
 *
 * Mirrors Flink's DAYOFYEAR(date) == EXTRACT(DOY FROM date) semantics:
 *   - OMNI_INT  : date, interpreted as days since epoch (1970-01-01). The day
 *                 of year is extracted in UTC via the Gregorian decomposition,
 *                 exactly like Flink's extractFromDate(DOY, days).
 *   - OMNI_LONG : Flink TIMESTAMP, represented as TimestampData = milliseconds
 *                 since epoch (NOT microseconds like the existing `year`).
 *                 No session timezone is applied — Flink's codegen for a plain
 *                 TIMESTAMP inlines extractFromDate(DOY, millis / 86400000).
 *                 The division is toward zero, so a negative timestamp that is
 *                 not exactly midnight lands on the next civil day.
 *                 TIMESTAMP_LTZ adds the session-zone offset first, then uses
 *                 the same day index.
 *
 * DOY formula: Flink julianExtract returns (julian - ymdToJulian(year,1,1)) + 1,
 * i.e. days since Jan 1 of the current year + 1. With std::tm.tm_yday in
 * [0..365] (0 = Jan 1) this is exactly `tm_yday + 1`, the form used here
 * (and by the existing DayOfYear.cpp).
 *
 * Returns NULL if the input is NULL, or (for OMNI_INT) when the date is so far
 * out of range that epochToCalendarUtc cannot decompose it.
 */

#include "FlinkDayOfYear.h"
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

/// Flink TIMESTAMP_LTZ shifts the instant by the session-zone offset, then
/// extracts with the same toward-zero day index as a plain TIMESTAMP. A null
/// zone is UTC (offset 0).
static int64_t SessionWallMillis(int64_t millis, bool hasTz, bool tzIsConst, const tz::TimeZone *constZone,
    BaseVector *tzArg, vector_size_t row)
{
    if (!hasTz) {
        return millis;
    }
    const tz::TimeZone *zone = constZone;
    if (!tzIsConst) {
        zone = (tzArg == nullptr || tzArg->IsNull(row))
            ? nullptr
            : ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, row));
    }
    if (zone == nullptr) {
        return millis;
    }
    return zone->to_local(tz::TimeZone::milliseconds{millis}).count();
}

static constexpr int64_t kSecondsPerDay = 86400LL;
static constexpr int64_t kMillisPerDay = 86400000LL;

/// flink_dayofyear function
/// flink_dayofyear(date) -> int32, flink_dayofyear(timestamp_millis) -> int32
/// OMNI_INT  = days since epoch (date); day of year extracted in UTC.
/// OMNI_LONG = milliseconds since epoch (Flink TimestampData); day of year
///             extracted in UTC (no session timezone — wall-clock semantics,
///             matching Flink EXTRACT(DOY FROM <TIMESTAMP>)).
/// Returns NULL if the input is NULL (or out of range for OMNI_INT).
class FlinkDayOfYearFunction : public VectorFunction {
public:
    explicit FlinkDayOfYearFunction(bool hasTz) : hasTz_(hasTz) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        // Arity comes from the registered signature (hasTz_), not args.size():
        // the eval stack is shared with sibling expressions.
        BaseVector *tzArg = nullptr;
        if (hasTz_) {
            tzArg = args.top();
            args.pop();
        }
        const auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
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
        bool tzIsConst = hasTz_ && (tzArg->GetEncoding() == OMNI_ENCODING_CONST);
        if (tzIsConst && !tzArg->IsNull(0)) {
            constZone = ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, 0));
        }

        if (inputTypeId == OMNI_INT) {
            // date = days since epoch; extract day of year in UTC (no timezone for DATE).
            auto *inputVector = reinterpret_cast<Vector<int32_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int32_t daysSinceEpoch = inputRaw[i];
                int64_t seconds = static_cast<int64_t>(daysSinceEpoch) * kSecondsPerDay;

                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    // tm_yday is 0-365 (0 = Jan 1); +1 gives 1-366, equal to Flink's
                    // (julian - ymdToJulian(year,1,1)) + 1.
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_yday + 1);
                    result->SetNotNull(i);
                } else {
                    // Out of supported range -> NULL.
                    result->SetNull(i);
                }
            });
        } else if (inputTypeId == OMNI_LONG) {
            // Flink TIMESTAMP = milliseconds since epoch (TimestampData).
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t millis = SessionWallMillis(inputRaw[i], hasTz_, tzIsConst, constZone, tzArg, i);
                int64_t days = millis / kMillisPerDay;
                int64_t seconds = days * kSecondsPerDay;
                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_yday + 1);
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        }
        delete inputArg;
        if (hasTz_) {
            delete tzArg;
        };
    }

private:
    const bool hasTz_;
};
} // namespace

void RegisterFlinkDayOfYearFunction(const std::string &name)
{
    auto func = std::make_shared<FlinkDayOfYearFunction>(false);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_LONG, func);
}

void RegisterFlinkDayOfYearWithTzFunction(const std::string &name)
{
    // _with_tz variant: same input types plus an explicit VARCHAR timezone
    // arg (appended by the OmniAdaptor for TIMESTAMP_WITH_LOCAL_TIME_ZONE).
    // The tz is only applied on the OMNI_LONG path; OMNI_INT (date) stays
    // in UTC. Reuses the same class; arity is encoded in hasTz_.
    auto func = std::make_shared<FlinkDayOfYearFunction>(true);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_LONG, func);
}
}
