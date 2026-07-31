/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_week function implementation
 *
 * flink_week(date) -> int32, flink_week(timestamp) -> int32
 *
 * Mirrors Flink's WEEK(date) == EXTRACT(WEEK FROM date) semantics:
 *   - OMNI_INT  : date, interpreted as days since epoch (1970-01-01). The ISO
 *                 8601 week number is computed in UTC, exactly like Flink's
 *                 extractFromDate(WEEK, days) -> getIso8601WeekNumber.
 *   - OMNI_LONG : Flink TIMESTAMP, represented as TimestampData = milliseconds
 *                 since epoch (NOT microseconds like the existing
 *                 `week_of_year`). No session timezone is applied — Flink's
 *                 codegen for a plain TIMESTAMP inlines
 *                 extractFromDate(WEEK, millis / 86400000), which treats the
 *                 stored millis as a wall-clock value. We achieve the same via
 *                 Timestamp::fromMillis(millis) -> getSeconds() and UTC
 *                 calendar decomposition.
 *
 * Week numbering: ISO 8601 (Monday-start, week 1 = the week containing the
 * year's first Thursday), range 1-53. This is NOT MySQL WEEK(date, 0).
 * The algorithm (getIsoWeekFromEpochSeconds) is ported from the existing
 * WeekOfYear.cpp and is mathematically equivalent to Flink's
 * getIso8601WeekNumber (both rely on the ISO rule that a week belongs to the
 * year of its Thursday). It correctly handles the late-December roll into
 * week 1 of the next year and the early-January roll into the last week of
 * the previous year.
 *
 * Returns NULL if the input is NULL, or when the date is so far out of range
 * that epochToCalendarUtc cannot decompose it.
 */

#include "FlinkWeek.h"
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
static constexpr int64_t kSecondsPerDay = 86400LL;

/// Compute ISO 8601 week number (1-53) from epoch seconds (UTC).
/// Ported from WeekOfYear.cpp::getIsoWeekFromEpochSeconds (same algorithm,
/// already covered by WeekOfYearTest). Mathematically equivalent to Flink's
/// DateTimeUtils.getIso8601WeekNumber: both use the ISO rule that a week
/// belongs to the year of its Thursday.
/// Returns true on success and sets `week`; false on conversion failure
/// (out of range etc.).
static bool getIsoWeekFromEpochSeconds(int64_t seconds, int32_t &week)
{
    std::tm tmValue;
    if (!Timestamp::epochToCalendarUtc(seconds, tmValue)) {
        return false;
    }
    // ISO weekday: 1=Monday .. 7=Sunday (tm_wday: 0=Sunday, 1=Monday, ...)
    int isoDow = (tmValue.tm_wday == 0) ? 7 : tmValue.tm_wday;
    int64_t days = seconds / kSecondsPerDay;
    if (seconds < 0 && seconds % kSecondsPerDay != 0) {
        days--;
    }
    // The Thursday of the current week determines the ISO week-year.
    int64_t thursdayDays = days - (isoDow - 4);

    std::tm tmThu;
    int64_t thursdayEpoch = thursdayDays * kSecondsPerDay;
    if (!Timestamp::epochToCalendarUtc(thursdayEpoch, tmThu)) {
        return false;
    }
    int yearThu = tmThu.tm_year + 1900;

    // Jan 4 of the week-year (always in ISO week 1 by definition).
    std::tm tmJan4 = {};
    tmJan4.tm_year = yearThu - 1900;
    tmJan4.tm_mon = 0;
    tmJan4.tm_mday = 4;
    tmJan4.tm_hour = 0;
    tmJan4.tm_min = 0;
    tmJan4.tm_sec = 0;
    tmJan4.tm_isdst = 0;
    int64_t jan4Epoch = Timestamp::calendarUtcToEpoch(tmJan4);
    std::tm tmJan4Filled;
    if (!Timestamp::epochToCalendarUtc(jan4Epoch, tmJan4Filled)) {
        return false;
    }
    int isoDowJan4 = (tmJan4Filled.tm_wday == 0) ? 7 : tmJan4Filled.tm_wday;
    int64_t jan4Days = jan4Epoch / kSecondsPerDay;
    if (jan4Epoch < 0 && jan4Epoch % kSecondsPerDay != 0) {
        jan4Days--;
    }
    // Monday of ISO week 1 of the week-year (the Monday of the week containing Jan 4).
    int64_t thursdayWeek1Days = jan4Days - (isoDowJan4 - 4);
    int64_t diff = thursdayDays - thursdayWeek1Days;
    week = static_cast<int32_t>(diff / 7) + 1;
    return true;
}

/// flink_week function
/// flink_week(date) -> int32, flink_week(timestamp_millis) -> int32
/// OMNI_INT  = days since epoch (date); ISO week extracted in UTC.
/// OMNI_LONG = milliseconds since epoch (Flink TimestampData); ISO week
///             extracted in UTC (no session timezone — wall-clock semantics,
///             matching Flink EXTRACT(WEEK FROM <TIMESTAMP>)).
/// Returns NULL if the input is NULL (or out of range).
class FlinkWeekFunction : public VectorFunction {
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
            // date = days since epoch; extract ISO week in UTC (no timezone for DATE).
            auto *inputVector = reinterpret_cast<Vector<int32_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int32_t daysSinceEpoch = inputRaw[i];
                int64_t seconds = static_cast<int64_t>(daysSinceEpoch) * kSecondsPerDay;
                int32_t w;
                if (getIsoWeekFromEpochSeconds(seconds, w)) {
                    resultRaw[i] = w;
                    result->SetNotNull(i);
                } else {
                    // Out of supported range -> NULL.
                    result->SetNull(i);
                }
            });
        } else if (inputTypeId == OMNI_LONG) {
            // Flink TIMESTAMP = milliseconds since epoch (TimestampData).
            // No session timezone: wall-clock semantics, matching Flink's
            // extractFromDate(WEEK, millis / 86400000).
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t millis = inputRaw[i];
                Timestamp ts = Timestamp::fromMillis(millis);
                // UTC seconds (no timezone shift) — wall-clock of the stored millis.
                int64_t seconds = ts.getSeconds();
                int32_t w;
                if (getIsoWeekFromEpochSeconds(seconds, w)) {
                    resultRaw[i] = w;
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

void RegisterFlinkWeekFunction(const std::string &name)
{
    // Only OMNI_INT (date = days since epoch) and OMNI_LONG (Flink TIMESTAMP =
    // millis since epoch) are supported, per the Flink WEEK semantics this
    // function mirrors. OMNI_LONG uses millisecond (not microsecond) units.
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<FlinkWeekFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_INT,
        std::make_shared<FlinkWeekFunction>());
}
}
