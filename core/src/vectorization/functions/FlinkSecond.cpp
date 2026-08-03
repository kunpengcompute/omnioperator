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
 * SECOND formula: Flink codegen = (millis % 60000) / 1000, range 0-59.
 * This is mathematically equal to std::tm.tm_sec obtained from
 * Timestamp::fromMillis(millis) -> getSeconds() -> epochToCalendarUtc (UTC):
 * the sub-millisecond part does not affect integer division by 1000, and
 * seconds%60 == (millis%60000)/1000. We use tm_sec here for consistency with
 * the other flink_* extractors (same fromMillis + UTC path).
 *
 * Only OMNI_LONG is registered (SECOND is meaningless on a DATE; Flink's
 * julianExtract throws AssertionError for SECOND on a date-only Julian day).
 *
 * Returns NULL if the input is NULL, or when the value is so far out of range
 * that epochToCalendarUtc cannot decompose it.
 */

#include "FlinkSecond.h"
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
/// flink_second function
/// flink_second(timestamp_millis) -> int32
/// OMNI_LONG = milliseconds since epoch (Flink TimestampData); second of minute
///             extracted in UTC (no session timezone — wall-clock semantics,
///             matching Flink EXTRACT(SECOND FROM <TIMESTAMP>)).
/// Returns integer 0-59. Returns NULL if the input is NULL (or out of range).
class FlinkSecondFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.empty()) {
            return;
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

        if (inputTypeId == OMNI_LONG) {
            // Flink TIMESTAMP = milliseconds since epoch (TimestampData).
            // No session timezone: wall-clock semantics, matching Flink's
            // (millis % 60000) / 1000.
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t millis = inputRaw[i];
                Timestamp ts = Timestamp::fromMillis(millis);
                // UTC seconds (no timezone shift) — wall-clock of the stored millis.
                int64_t seconds = ts.getSeconds();
                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    // tm_sec is 0-59, equal to Flink's (millis % 60000) / 1000.
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_sec);
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        }
        delete inputArg;
    }
};
} // namespace

void RegisterFlinkSecondFunction(const std::string &name)
{
    // Only OMNI_LONG (Flink TIMESTAMP = millis since epoch) is supported, per
    // the Flink SECOND semantics this function mirrors. SECOND is a timestamp
    // extractor (not applicable to DATE). Returns an INTEGER second (0-59), not
    // the Decimal with-fraction variant. OMNI_LONG uses millisecond (not
    // microsecond) units.
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_INT,
        std::make_shared<FlinkSecondFunction>());
}
}
