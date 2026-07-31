/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_month function implementation
 *
 * flink_month(date) -> int32, flink_month(timestamp) -> int32
 *
 * Mirrors Flink's MONTH(date) == EXTRACT(MONTH FROM date) semantics:
 *   - OMNI_INT  : date, interpreted as days since epoch (1970-01-01). The
 *                 month is extracted in UTC via the Gregorian decomposition,
 *                 exactly like Flink's extractFromDate(MONTH, days).
 *   - OMNI_LONG : Flink TIMESTAMP, represented as TimestampData = milliseconds
 *                 since epoch (NOT microseconds like the existing `year`).
 *                 No session timezone is applied — Flink's codegen for a plain
 *                 TIMESTAMP inlines extractFromDate(MONTH, millis / 86400000),
 *                 which treats the stored millis as a wall-clock value. We
 *                 achieve the same via Timestamp::fromMillis(millis) followed
 *                 by UTC calendar decomposition (GetDateTime with nullptr tz).
 *
 * Month formula: Flink julianExtract returns `month` directly (month in
 * [1..12]). With std::tm.tm_mon in [0..11] (month = tm_mon + 1) this is
 * exactly `tm_mon + 1`, the form used here (and by the existing Month.cpp).
 *
 * Returns NULL if the input is NULL, or (for OMNI_INT) when the date is so far
 * out of range that epochToCalendarUtc cannot decompose it.
 */

#include "FlinkMonth.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/TimeUtils.h"
#include <ctime>
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
static constexpr int64_t kSecondsPerDay = 86400LL;

/// flink_month function
/// flink_month(date) -> int32, flink_month(timestamp_millis) -> int32
/// OMNI_INT  = days since epoch (date); month extracted in UTC.
/// OMNI_LONG = milliseconds since epoch (Flink TimestampData); month
///             extracted in UTC (no session timezone — wall-clock semantics,
///             matching Flink EXTRACT(MONTH FROM <TIMESTAMP>)).
/// Returns NULL if the input is NULL (or out of range for OMNI_INT).
class FlinkMonthFunction : public VectorFunction {
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

        if (inputTypeId == OMNI_INT) {
            // date = days since epoch; extract month in UTC (no timezone for DATE).
            auto *inputVector = reinterpret_cast<Vector<int32_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int32_t daysSinceEpoch = inputRaw[i];
                int64_t seconds = static_cast<int64_t>(daysSinceEpoch) * kSecondsPerDay;

                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    // tm_mon is 0-11; +1 gives 1-12, equal to Flink's `return month`.
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_mon + 1);
                    result->SetNotNull(i);
                } else {
                    // Out of supported range -> NULL.
                    result->SetNull(i);
                }
            });
        } else if (inputTypeId == OMNI_LONG) {
            // Flink TIMESTAMP = milliseconds since epoch (TimestampData).
            // No session timezone: wall-clock semantics, matching Flink's
            // extractFromDate(MONTH, millis / 86400000).
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t millis = inputRaw[i];
                Timestamp ts = Timestamp::fromMillis(millis);
                // nullptr timezone => decompose in UTC (wall-clock of the stored millis).
                std::tm tmValue = util::GetDateTime(ts, /*timeZone=*/nullptr);
                resultRaw[i] = static_cast<int32_t>(tmValue.tm_mon + 1);
                result->SetNotNull(i);
            });
        }
        delete inputArg;
    }
};
} // namespace

void RegisterFlinkMonthFunction(const std::string &name)
{
    // Only OMNI_INT (date = days since epoch) and OMNI_LONG (Flink TIMESTAMP =
    // millis since epoch) are supported, per the Flink MONTH semantics this
    // function mirrors. OMNI_LONG uses millisecond (not microsecond) units.
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<FlinkMonthFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_INT,
        std::make_shared<FlinkMonthFunction>());
}
}
