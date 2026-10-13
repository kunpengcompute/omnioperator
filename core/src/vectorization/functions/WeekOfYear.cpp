/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: week_of_year(date/timestamp) -> int (ISO week 1-53), vectorized implementation
 */

#include "WeekOfYear.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>
#include "libboundscheck/include/securec.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
static constexpr int64_t kSecondsPerDay = 86400LL;
static constexpr int64_t kMicrosecondsPerSecond = 1000000LL;

/// Compute ISO 8601 week number (1-53) from epoch seconds (UTC).
/// Returns true on success and sets week; false on conversion failure (out of range etc.).
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
    int64_t thursdayDays = days - (isoDow - 4);

    std::tm tmThu;
    int64_t thursdayEpoch = thursdayDays * kSecondsPerDay;
    if (!Timestamp::epochToCalendarUtc(thursdayEpoch, tmThu)) {
        return false;
    }
    int yearThu = tmThu.tm_year + 1900;

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
    int64_t thursdayWeek1Days = jan4Days - (isoDowJan4 - 4);
    int64_t diff = thursdayDays - thursdayWeek1Days;
    week = static_cast<int32_t>(diff / 7) + 1;
    return true;
}

class WeekOfYearFunction : public VectorFunction {
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
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }
        if (result->GetSize() < size) {
            delete inputArg;
            OMNI_THROW("WeekOfYearFunction Error:", "Result vector size is less than input size: " +
                std::to_string(result->GetSize()) + " < " + std::to_string(size));
        }

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        const auto inputTypeId = inputArg->GetTypeId();

        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy(resultNulls, inputNulls, nullsSize);

        SelectivityVector rows(size);
        rows.setFromBitsNegate(inputNulls, size);

        if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
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
                    result->SetNull(i);
                }
            });
        } else if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
            rows.applyToSelected([&](vector_size_t i) {
                int64_t micros = inputRaw[i];
                int64_t seconds = micros / kMicrosecondsPerSecond;
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

void RegisterWeekOfYearFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_INT,
        std::make_shared<WeekOfYearFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<WeekOfYearFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_INT,
        std::make_shared<WeekOfYearFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_INT,
        std::make_shared<WeekOfYearFunction>());
}
}
