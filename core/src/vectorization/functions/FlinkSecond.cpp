/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_second function implementation
 *
 * flink_second(timestamp_millis) -> int32
 *
 * Mirrors Flink's SECOND(timestamp) == EXTRACT(SECOND FROM timestamp) semantics:
 *   - OMNI_LONG : Flink TIMESTAMP, represented as TimestampData = milliseconds
 *                 since epoch (NOT microseconds like the existing `second`).
 *                 No session timezone is applied — Flink's codegen for a plain
 *                 TIMESTAMP inlines `ts.getMillisecond() % 60000 / 1000`
 *                 (pure modular arithmetic, no calendar decomposition, no tz).
 *
 * SECOND is a "timestamp extractor" (docs: "from SQL timestamp"), in the same
 * family as HOUR/MINUTE. Like them it does NOT apply the session timezone for
 * a plain TIMESTAMP: the stored millis are treated as a UTC wall-clock value.
 * (Session tz is applied only for TIMESTAMP_WITH_LOCAL_TIME_ZONE, via a
 * different codegen path.) Tests confirm this: EXTRACT(SECOND FROM f18)=44
 * for TIMESTAMP '1996-11-10 06:55:44.333'.
 *
 * This returns an INTEGER second (0-59), matching Flink's SECOND() / Calcite's
 * SqlDatePartFunction.SECOND. It is NOT the Decimal(8,6) with-fraction variant
 * (SecondWithFractionFunction in Second.cpp) — that is a different extraction
 * unit (SECOND WITH FRACTION), not what Flink's SECOND() function returns.
 *
 * SECOND formula for a plain TIMESTAMP (ExtractCallGen):
 * (millis % 60000) / 1000, both operations toward zero, the same as Java.
 * For millis < 0 the remainder is negative, so the second can be negative
 * and is not std::tm.tm_sec. The _with_tz path still decomposes the instant
 * in the session zone (TIMESTAMP_LTZ).
 *
 * Only OMNI_LONG is registered (SECOND is meaningless on a DATE; Flink's
 * julianExtract throws AssertionError for SECOND on a date-only Julian day).
 *
 * Returns NULL if the input is NULL, or when the value is so far out of range
 * that epochToCalendarUtc cannot decompose it.
 */

#include "FlinkSecond.h"
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
static constexpr int64_t kMillisPerSecond = 1000LL;
static constexpr int64_t kMillisPerMinute = 60000LL;

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

/// flink_second function
/// flink_second(timestamp_millis) -> int32
/// OMNI_LONG = milliseconds since epoch (Flink TimestampData); second of minute
///             extracted in UTC (no session timezone — wall-clock semantics,
///             matching Flink EXTRACT(SECOND FROM <TIMESTAMP>)).
/// Returns integer 0-59. Returns NULL if the input is NULL (or out of range).
class FlinkSecondFunction : public VectorFunction {
public:
    explicit FlinkSecondFunction(bool hasTz) : hasTz_(hasTz) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        // Arity comes from the registered signature (hasTz_), not args.size():
        // the eval stack is shared with sibling expressions.
        // flink_X:         args top = [input]
        // flink_X_with_tz: args top = [input, tz]
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

        if (inputTypeId == OMNI_LONG) {
            // Flink TIMESTAMP = milliseconds since epoch (TimestampData).
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t millis = inputRaw[i];
                if (!hasTz_) {
                    resultRaw[i] = (millis % kMillisPerMinute) / kMillisPerSecond;
                    result->SetNotNull(i);
                    return;
                }
                Timestamp ts = Timestamp::fromMillis(millis);
                const tz::TimeZone *zone = constZone;
                if (!tzIsConst) {
                    zone = tzArg->IsNull(i) ? nullptr
                        : ResolveSessionTimeZone(VectorHelper::GetStringValueFromVector(tzArg, i));
                }
                std::tm tmValue = util::GetDateTime(ts, zone);
                resultRaw[i] = static_cast<int32_t>(tmValue.tm_sec);
                result->SetNotNull(i);
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

void RegisterFlinkSecondFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_LONG,
        std::make_shared<FlinkSecondFunction>(false));
}

void RegisterFlinkSecondWithTzFunction(const std::string &name)
{
    // _with_tz variant: OMNI_LONG plus an explicit VARCHAR timezone arg
    // (appended by the OmniAdaptor for TIMESTAMP_WITH_LOCAL_TIME_ZONE).
    // Reuses the same class; arity is encoded in hasTz_.
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_LONG,
        std::make_shared<FlinkSecondFunction>(true));
}
}
